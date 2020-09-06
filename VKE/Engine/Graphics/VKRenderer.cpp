#include "VKRenderer.h"

#include "ComputePass.h"
// Engine
#include "Camera.h"
#include "Mesh/Mesh.h"
#include "Texture/Texture.h"
#include "Transform/Transform.h"
#include "Model/Model.h"
#include "Descriptors/Descriptor_Buffer.h"
#include "Descriptors/Descriptor_Dynamic.h"
#include "Descriptors/Descriptor_Image.h"

// system
#include <stdexcept>
#include "stdlib.h"
#include <set>
#include "assert.h"

// glm
#include "glm/gtc/matrix_transform.hpp"

// assimp
#include "assimp/Importer.hpp"
#include <assimp/scene.h>
#include "assimp/postprocess.h"

// imgui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_glfw.h"
namespace VKE
{

	//** Global Variables * /
	std::shared_ptr<cModel> GQuadModel = nullptr;

	int VKRenderer::init(GLFWwindow* iWindow)
	{
		window = iWindow;
		try
		{
			createInstance();
			createSurface();
			getPhysicalDevice();
			createLogicalDevice();
			createSwapChain();
			createFrameBufferImage();		// Need to get depth buffer image format before creating a render pass that needs a depth attachment
			createRenderPass();
			createFrameBuffer();
			createCommandPool();
			createCommandBuffers();
			createSynchronization();
			
			// Descriptor set and push constant related
			{		
				// Create Texture
				cTexture::Load("DefaultWhite.png", MainDevice);
				cTexture::Load("fireParticles/TXT_Sparks_01.tga", MainDevice);
				CreateDescriptorSets();
				createPushConstantRange();
			}
			LoadAssets();
			createGraphicsPipeline();
			// Create compute pass
			pCompute = DBG_NEW FComputePass();
			if (pCompute)
				pCompute->init(&MainDevice);
		}
		catch (const std::runtime_error &e)
		{
			printf("ERROR: %s \n", e.what());
			return EXIT_FAILURE;
		}


		return EXIT_SUCCESS;
	}

	void VKRenderer::tick(float dt)
	{
		if (RenderList.size() > 0 && RenderList[0])
		{
			RenderList[0]->Transform.gRotate(cTransform::WorldUp, dt);
			RenderList[0]->Transform.Update();
		}

		if (pCompute)
		{
			pCompute->ParticleSupportData.dt = dt;
			//pCompute->Emitter.Transform.gRotate(cTransform::WorldRight, dt);
			//pCompute->Emitter.Transform.Update();
		}
	}


	VkResult VKRenderer::prepareForDraw()
	{
		int CurrentFrame = ElapsedFrame % MAX_FRAME_DRAWS;
		// Wait for given fence to be opened from last draw before continuing
		vkWaitForFences(MainDevice.LD, 1, &DrawFences[CurrentFrame],
			VK_TRUE,													// Must wait for all fences to be opened(signaled) to pass this wait
			std::numeric_limits<uint64_t>::max());						// No time-out
		vkResetFences(MainDevice.LD, 1, &DrawFences[CurrentFrame]);		// Need to close(reset) this fence manually

		/** get the next available image to draw to and signal(semaphore1) when we're finished with the image */
		return SwapChain.acquireNextImage(MainDevice, OnImageAvailables[CurrentFrame]);
	}

	VkResult VKRenderer::presentFrame()
	{
		int CurrentFrame = ElapsedFrame % MAX_FRAME_DRAWS;
		VkPresentInfoKHR PresentInfo = {};
		PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		PresentInfo.waitSemaphoreCount = 1;
		PresentInfo.pWaitSemaphores = &OnRenderFinisheds[CurrentFrame];	// Semaphores to wait on
		PresentInfo.swapchainCount = 1;									// Number of swap chains to present to
		PresentInfo.pSwapchains = &SwapChain.SwapChain;					// Swap chains to present image to
		PresentInfo.pImageIndices = &SwapChain.ImageIndex;				// Index of images in swap chains to present

		VkResult Result = vkQueuePresentKHR(MainDevice.presentationQueue, &PresentInfo);
		if (!((Result == VK_SUCCESS) || (Result == VK_SUBOPTIMAL_KHR))) {
			if (Result == VK_ERROR_OUT_OF_DATE_KHR) {
				// Swap chain is no longer compatible with the surface and needs to be recreated
				recreateSwapChain();
				return Result;
			}
			else {
				RESULT_CHECK(Result, "failed to present swap chain image!");
				return Result;
			}
		}

		Result = vkQueueWaitIdle(MainDevice.presentationQueue);
		RESULT_CHECK(Result, "Fail to wait for queue");
		return Result;
	}

	void VKRenderer::postPresentationStage()
	{
		int CurrentFrame = ElapsedFrame % MAX_FRAME_DRAWS;
		// Wait for rendering finished
		VkPipelineStageFlags ComputeWaitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		// Submit compute commands
		VkSubmitInfo ComputeSubmitInfo = {};
		ComputeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		ComputeSubmitInfo.commandBufferCount = 1;
		ComputeSubmitInfo.pCommandBuffers = &pCompute->CommandBuffer;
		ComputeSubmitInfo.waitSemaphoreCount = 1;
		ComputeSubmitInfo.pWaitSemaphores = &OnGraphicFinished[CurrentFrame];
		ComputeSubmitInfo.pWaitDstStageMask = &ComputeWaitStageMask;
		ComputeSubmitInfo.signalSemaphoreCount = 1;
		ComputeSubmitInfo.pSignalSemaphores = &pCompute->OnComputeFinished;
		VkResult Result = vkQueueSubmit(pCompute->ComputeQueue, 1, &ComputeSubmitInfo, DrawFences[CurrentFrame]);
		RESULT_CHECK(Result, "Fail to submit compute command buffers to compute queue");
	}

	void VKRenderer::draw()
	{
		int CurrentFrame = ElapsedFrame % MAX_FRAME_DRAWS;
		VkResult PrepareResult = prepareForDraw();
		// Swap chain is out of date
		if ((PrepareResult == VK_ERROR_OUT_OF_DATE_KHR) || (PrepareResult == VK_SUBOPTIMAL_KHR))
		{
			recreateSwapChain();
			return;
		}
		else if (PrepareResult != VK_SUCCESS && PrepareResult != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		// Record graphic commands
		recordCommands();
		// Update uniform buffer
		updateUniformBuffers();

		const uint32_t GraphicWaitSemaphoreCount = 2;
		VkSemaphore GraphicsWaitSemaphores[] = { pCompute->OnComputeFinished, OnImageAvailables[CurrentFrame] };
		const uint32_t GraphicSignalSemaphoreCount = 2;
		VkSemaphore GraphicsSignalSemaphores[] = { OnGraphicFinished[CurrentFrame], OnRenderFinisheds[CurrentFrame] };

		/** II. submit command buffer to queue (graphic queue) for execution, make sure it waits for the image to be signaled as available before drawing,
		 and signals (semaphore2) when it has finished rendering.*/
		 // Queue Submission information
		VkSubmitInfo SubmitInfo = {};
		SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		SubmitInfo.waitSemaphoreCount = GraphicWaitSemaphoreCount;							// Number of semaphores to wait on
		SubmitInfo.pWaitSemaphores = GraphicsWaitSemaphores;								// It can start the command buffers all the way before it can draw to the screen.
		VkPipelineStageFlags WaitStages[] =
		{
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT		// Seen this in subpass dependency, command buffers will run until this stage
		};
		SubmitInfo.pWaitDstStageMask = WaitStages;											// Stages to check semaphores at
		SubmitInfo.commandBufferCount = 1;													// Number of command buffers to submit, only submit to one frame once
		SubmitInfo.pCommandBuffers = &CommandBuffers[SwapChain.ImageIndex];					// Command buffer to submit
		SubmitInfo.signalSemaphoreCount = GraphicSignalSemaphoreCount;						// Number of Semaphores to signal before the command buffer has finished
		SubmitInfo.pSignalSemaphores = GraphicsSignalSemaphores;							// When the command buffer is finished, these semaphores will be signaled

		// This is the execute function also because the queue will execute commands automatically
		// When finish those commands, open(signal) this fence
		VkResult Result = vkQueueSubmit(MainDevice.graphicQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
		RESULT_CHECK(Result, "Fail to submit command buffers to graphic queue");

		/** III. present image to screen when it has signaled finished rendering */
		Result = presentFrame();

		/** IV. Submit compute queue*/
		postPresentationStage();

		// Increment Elapsed Frame
		++ElapsedFrame;
	}

	void VKRenderer::cleanUp()
	{
		// wait until the device is not doing anything (nothing on any queue)
		vkDeviceWaitIdle(MainDevice.LD);

		// Cleanup compute pass
		if (pCompute)
			pCompute->cleanUp();
		safe_delete(pCompute);

		cTexture::Free();
		// Clean up render list
		for (auto Model : RenderList)
		{
			Model->cleanUp();
		}
		RenderList.clear();

		GQuadModel->cleanUp();		

		// Clear all mesh assets
		cMesh::Free();

		for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
		{
			vkDestroySemaphore(MainDevice.LD, OnGraphicFinished[i], nullptr);
			vkDestroySemaphore(MainDevice.LD, OnRenderFinisheds[i], nullptr);
			vkDestroySemaphore(MainDevice.LD, OnImageAvailables[i], nullptr);
			vkDestroyFence(MainDevice.LD, DrawFences[i], nullptr);
		}

		// clean up depth buffer
		{
			for (size_t i = 0; i < SwapChain.Images.size(); ++i)
			{
				ColorBuffers[i].cleanUp();
				DepthBuffers[i].cleanUp();
			}

			ColorBuffers.clear();
			DepthBuffers.clear();
		}
		// Descriptor related
		{
			vkDestroyDescriptorPool(MainDevice.LD, DescriptorPool, nullptr);
			vkDestroyDescriptorPool(MainDevice.LD, SamplerDescriptorPool, nullptr);
			for (size_t i = 0; i < SwapChain.Images.size(); ++i)
			{
				DescriptorSets[i].cleanUp();
				
				InputDescriptorSets[i].cleanUp();
			}
			ParticleDescriptorSet.cleanUp();
			cDescriptorSet::CleanupDescriptorSetLayout(&MainDevice);
		}

		cleanupSwapChain();

		vkDestroyCommandPool(MainDevice.LD, MainDevice.GraphicsCommandPool, nullptr);

		vkDestroySurfaceKHR(vkInstance, Surface, nullptr);
		vkDestroyDevice(MainDevice.LD, nullptr);
		vkDestroyInstance(vkInstance, nullptr);
	}

	void VKRenderer::LoadAssets()
	{
		// Create Mesh
		std::shared_ptr<cModel> pContainerModel = nullptr;
		std::shared_ptr<cModel> pPlaneModel = nullptr;

		/*CreateModel("Container.obj", pContainerModel);
		RenderList.push_back(pContainerModel);
		pContainerModel->Transform.SetTransform(glm::vec3(0, -2, -5), glm::quat(1, 0, 0, 0), glm::vec3(0.01f, 0.01f, 0.01f));*/

		/*CreateModel("Plane.obj", pPlaneModel);
		RenderList.push_back(pPlaneModel);
		pPlaneModel->Transform.SetTransform(glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), glm::vec3(25, 25, 25));*/
		
		CreateModel("Quad.obj", GQuadModel);
		GQuadModel->Transform.SetTransform(glm::vec3(0, 0, 0), glm::quat(1, 0, 0, 0), glm::vec3(1, 1, 1));

	}

	void VKRenderer::createInstance()
	{
		if (EnableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("validation layers requested, but not available!");
		}
		// Create a application info
		// Most data here doesn't affect the program and is for developer convenience
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan App";					// Application name
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);		// custom application version
		appInfo.pEngineName = "VKE";								// Custom engine name
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);			// Custom engine version
		appInfo.apiVersion = VK_API_VERSION_1_2;					// Vulkan version

		// Create information for a VKInstance
		VkInstanceCreateInfo CreateInfo = {};

		CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;	// VK_STRUCTURE_TYPE + xxxInfo that you want, in this case: InstanceCreateInfo
		CreateInfo.pNext = nullptr;									// this will point to extended information, can be another struct
		/*CreateInfo.flags = VK_WHATEVER | VK_OTHER;*/
		CreateInfo.pApplicationInfo = &appInfo;


		// Create list to hold instance extensions
		uint32_t glfwExtensionCount = 0;							// GLFW may require multiple extensions;
		const char** glfwExtensions;								// Extensions passed as array of c-strings, so need the array to pointer

		// Get glfw extensions
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// Check instance Extensions supported
		if (!checkInstanceExtensionSupport(glfwExtensions, glfwExtensionCount))
		{
			throw std::runtime_error("VKInstance does not support required extensions!");
		}

		CreateInfo.enabledExtensionCount = glfwExtensionCount;
		CreateInfo.ppEnabledExtensionNames = glfwExtensions;

		if (EnableValidationLayers)
		{
			CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
			CreateInfo.ppEnabledLayerNames = ValidationLayers.data();
		}
		else
		{
			CreateInfo.enabledLayerCount = 0;							// validation layer count
			CreateInfo.ppEnabledLayerNames = nullptr;					// @TODO: Setup validation layers that instance will use
		}

		// VkAllocationCallbacks is a custom allocator, not using here
		VkResult Result = vkCreateInstance(&CreateInfo, nullptr, &vkInstance);
		RESULT_CHECK(Result, "Fail to Create VKInstance");
	}

	void VKRenderer::getPhysicalDevice()
	{
		uint32_t DeviceCount = 0;
		vkEnumeratePhysicalDevices(vkInstance, &DeviceCount, nullptr);

		printf("Physical Device Count: %i\n", DeviceCount);
		// if no devices available, Vulkan not supported
		if (DeviceCount == 0)
		{
			throw std::runtime_error("Cannot find any GPU support Vulkan Instance");
		}

		std::vector<VkPhysicalDevice> devices(DeviceCount);
		vkEnumeratePhysicalDevices(vkInstance, &DeviceCount, devices.data());

		// Pick first device first
		for (const auto& device : devices)
		{
			if (checkDeviceSuitable(device))
			{
				MainDevice.PD = device;
				break;
			}
		}
		if (MainDevice.PD == nullptr)
		{
			throw std::runtime_error("Cannot find any suitable physical device");
		}

		// Information about the device itself (ID, name, type, vendor, etc)
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(MainDevice.PD, &deviceProperties);
		SetMinUniformOffsetAlignment(deviceProperties.limits.minUniformBufferOffsetAlignment);
		printf("Min Uniform Buffer Offset Alignment: %d\n", static_cast<int>(GetMinUniformOffsetAlignment()));
	}

	void VKRenderer::createLogicalDevice()
	{
		// Queue is needed to create a logical device,
		// vector for queue create information
		std::vector< VkDeviceQueueCreateInfo> queueCreateInfos;
		// set of queue family indices, prevent duplication
		std::set<int> queueFamilyIndices = { MainDevice.QueueFamilyIndices.graphicFamily, MainDevice.QueueFamilyIndices.presentationFamily, MainDevice.QueueFamilyIndices.computeFamily };
		const float DefaultPriority = 0.0f;
		for (auto& queueFamilyIdx : queueFamilyIndices)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamilyIdx;											// Index of the family to create a queue from
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &DefaultPriority;										// If there are multiple queues, GPU needs to know the execution order of different queues (1.0 == highest prioity)
			queueCreateInfos.push_back(queueCreateInfo);
		}

		// Info to create a (logical) device
		VkDeviceCreateInfo DeviceCreateInfo = {};
		DeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		DeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());			// Number of queues in this device.
		DeviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();									// list of queue create infos so that the devices will create required queues.
		DeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());		// Number of enabled logical device extensions
		DeviceCreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();								// List of logical device extensions

		// Physical Device Features the Logical Device will use
		VkPhysicalDeviceFeatures PDFeatures = {};
		PDFeatures.depthClamp = VK_TRUE;
		PDFeatures.samplerAnisotropy = VK_TRUE;

		DeviceCreateInfo.pEnabledFeatures = &PDFeatures;

		VkResult Result = vkCreateDevice(MainDevice.PD, &DeviceCreateInfo, nullptr, &MainDevice.LD);
		RESULT_CHECK(Result, "Fail to Create VKLogical Device");

		// After LD is created, queues should be created too, Save the queues
		vkGetDeviceQueue(
			/*From given LD*/MainDevice.LD,
			/*given queue family*/ MainDevice.QueueFamilyIndices.graphicFamily,
			/*given queue index(0 only one queue)*/ 0,
			/*Out queue*/ &MainDevice.graphicQueue);
		vkGetDeviceQueue(MainDevice.LD, MainDevice.QueueFamilyIndices.presentationFamily, 0, &MainDevice.presentationQueue);
	}

	void VKRenderer::createSurface()
	{
		/* An example how to create platform specific Surface*/
		/*/ Use win32 platform
		VkWin32SurfaceCreateInfoKHR Win32SurfaceCreateInfo;
		vkCreateWin32SurfaceKHR()
		*/

		// Create surface including:
		// create a surface info struct,
		// runs the create surface function, return result
		VkResult Result = glfwCreateWindowSurface(vkInstance, window, nullptr, &Surface);
		RESULT_CHECK(Result, "Fail to create a surface");
	}

	void VKRenderer::createSwapChain()
	{
		getSwapChainDetail(MainDevice.PD);
		// Get Parameters for SwapChain
		VkSurfaceFormatKHR SurfaceFormat = SwapChainDetail.getSurfaceFormat();
		VkPresentModeKHR PresentMode = SwapChainDetail.getPresentationMode();
		VkExtent2D Resolution = SwapChainDetail.getSwapExtent();

		VkSwapchainCreateInfoKHR SwapChainCreateInfo = {};
		SwapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		SwapChainCreateInfo.surface = Surface;

		/** Major values */
		SwapChainCreateInfo.imageFormat = SurfaceFormat.format;
		SwapChainCreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
		SwapChainCreateInfo.presentMode = PresentMode;
		SwapChainCreateInfo.imageExtent = Resolution;
		if (Resolution.width == 0)
		{
			printf("illegal");
		}
		/** Other values */
		// Get 1 extra image to allow triple buffering
		uint32_t MinImageCount = SwapChainDetail.SurfaceCapabilities.minImageCount + 1;

		// In this case, min == max && they are not 0 (unlimited), clamp the MinImageCount
		if (SwapChainDetail.SurfaceCapabilities.maxImageCount > 0 && SwapChainDetail.SurfaceCapabilities.maxImageCount < MinImageCount)
		{
			MinImageCount = SwapChainDetail.SurfaceCapabilities.maxImageCount;
		}
		SwapChainCreateInfo.minImageCount = MinImageCount;											// Minimum images in a SwapChain
		SwapChainCreateInfo.imageArrayLayers = 1;													// Number of layers for each image in chain
		SwapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;						// What attachment images will be used as, SwapChain only move image around, depth data should be in other things
		SwapChainCreateInfo.preTransform = SwapChainDetail.SurfaceCapabilities.currentTransform;	// Transform to perform on swap image
		SwapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						// How to handle blending image with external graphics (e.g. other window)
		SwapChainCreateInfo.clipped = VK_TRUE;														// Whether to clip parts of image not in view (e.g. behind another window)

		uint32_t QueueFamilyIndices[] =
		{
			static_cast<uint32_t>(MainDevice.QueueFamilyIndices.graphicFamily),
			static_cast<uint32_t>(MainDevice.QueueFamilyIndices.presentationFamily),
		};

		// Images are sharing between two queues
		if (MainDevice.QueueFamilyIndices.graphicFamily != MainDevice.QueueFamilyIndices.presentationFamily)
		{
			SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;						// Image share handling
			SwapChainCreateInfo.queueFamilyIndexCount = 2;											// Number of queues sharing image between
			SwapChainCreateInfo.pQueueFamilyIndices = QueueFamilyIndices;							// Array of queues to share between
		}
		else
		{
			// No need to share between queues
			SwapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			SwapChainCreateInfo.queueFamilyIndexCount = 0;
			SwapChainCreateInfo.pQueueFamilyIndices = nullptr;
		}

		SwapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;											// Can use the old SwapChain data to create this new one, very useful when resize the window

		VkResult Result = vkCreateSwapchainKHR(MainDevice.LD, &SwapChainCreateInfo, nullptr, &SwapChain.SwapChain);
		RESULT_CHECK(Result, "Fail to create SwapChain");

		/** Store created data */
		SwapChain.ImageFormat = SwapChainCreateInfo.imageFormat;
		SwapChain.Extent = SwapChainCreateInfo.imageExtent;

		uint32_t SwapChainImageCount;
		vkGetSwapchainImagesKHR(MainDevice.LD, SwapChain.SwapChain, &SwapChainImageCount, nullptr);
		std::vector<VkImage> Images(SwapChainImageCount);
		vkGetSwapchainImagesKHR(MainDevice.LD, SwapChain.SwapChain, &SwapChainImageCount, Images.data());

		SwapChain.Images.resize(SwapChainImageCount);

		for (size_t i = 0; i < Images.size(); ++i)
		{
			FSwapChainImage SwapChainImage = {};
			SwapChainImage.Image = Images[i];
			SwapChainImage.ImgView = CreateImageViewFromImage(&MainDevice, Images[i], SwapChain.ImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

			SwapChain.Images[i] = SwapChainImage;
		}
		assert(MAX_FRAME_DRAWS <= Images.size());
		printf("%d Image view has been created\n", SwapChainImageCount);
	}

	void VKRenderer::createRenderPass()
	{
		// Array of sub-passes
		const uint32_t SubpassCount = 3;
		VkSubpassDescription Subpasses[SubpassCount];

		/** 1. Sub-pass 0 attachments (input attachments) */
		VkAttachmentDescription ColorAttachment = {};
		// COLOR ATTACHMENT (input)
		ColorAttachment.format = ColorBuffers[0].GetFormat();
		ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;					// Clear at the beginning of the render pass, not sub-pass
		ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// What to do after the render pass is finished. At that time this attachment should not be cared any more
		ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// Final layout of when it is at the end of the render pass

		// DEPTH_ATTACHMENT (input)
		VkAttachmentDescription DepthAttachment = {};
		DepthAttachment.format = DepthBuffers[0].GetFormat();
		DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;					// Don't care about storing the depth
		DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Sub-pass 0 references
		VkAttachmentReference ColorAttachmentReference = {};
		ColorAttachmentReference.attachment = 1;
		ColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference DepthAttachmentReference = {};
		DepthAttachmentReference.attachment = 2;
		DepthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		// Setup sub-pass 0
		Subpasses[0] = Helpers::SubpassDescriptionDefault(VK_PIPELINE_BIND_POINT_GRAPHICS);
		Subpasses[0].colorAttachmentCount = 1;
		Subpasses[0].pColorAttachments = &ColorAttachmentReference;
		Subpasses[0].pDepthStencilAttachment = &DepthAttachmentReference;

		// Setup sub-pass 1, using the same as the first pass
		Subpasses[1] = Helpers::SubpassDescriptionDefault(VK_PIPELINE_BIND_POINT_GRAPHICS);
		Subpasses[1].colorAttachmentCount = 1;
		Subpasses[1].pColorAttachments = &ColorAttachmentReference;
		Subpasses[1].pDepthStencilAttachment = &DepthAttachmentReference;

		/** 2. Sub-pass 2 attachments (input attachments) */
		VkAttachmentDescription SwapChainColorAttachment = {};

		// SWAPCHAIN_COLOR_ATTACHMENT_1
		SwapChainColorAttachment.format = SwapChain.ImageFormat;								// Format to use for this attachment
		SwapChainColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;								// Number of samples to write for Sampling
		SwapChainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;							// Define what happen before rendering
		SwapChainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;						// Define what happen after rendering
		SwapChainColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;				// Don't care about stencil before rendering
		SwapChainColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;				// Don't care about stencil after rendering
		// Frame buffer data will be stored as an image, but images can be given different data layouts
		// to give optimal use for certain operations

		// Assume there are multiple sub-passes and all need to process the colorAttachment, its data layout may change between different sub-passes.
		// After all sub-passes has done with their work, the attachment should change to the finalLayout.
		// Initial layout -> subpasses1 layout -> ... -> subpassN layout -> final layout
		SwapChainColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;					// Image data layout before render pass starts
		SwapChainColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;				// Image data layout after render pass (to change to) for final purpose

		// Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
		VkAttachmentReference SwapChainColorAttachmentReference = {};
		SwapChainColorAttachmentReference.attachment = 0;										// It can refer to the same attachment, but with different layout
		SwapChainColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// optimal layout for color attachment

		/** 2.5 Create Input attachments from sub-pass 0 to sub-pass 1 (not necessary if no attachment need to be referenced in the incoming pass) */
		// Color and Depth from sub-pass 0
		const uint32_t InputReferenceCount = 2;
		VkAttachmentReference InputReferences[InputReferenceCount];
		InputReferences[0].attachment = 1;														// Same index order as in frame buffer's attachments, this is Color
		InputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		InputReferences[1].attachment = 2;														// This is Depth
		InputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// Setup sub-pass 2
		Subpasses[2] = Helpers::SubpassDescriptionDefault(VK_PIPELINE_BIND_POINT_GRAPHICS);
		Subpasses[2].colorAttachmentCount = 1;
		Subpasses[2].pColorAttachments = &SwapChainColorAttachmentReference;				// Color attachment references
		Subpasses[2].inputAttachmentCount = InputReferenceCount;
		Subpasses[2].pInputAttachments = InputReferences;

		/** 3. Need to determine when layout transitions occur using sub-pass dependencies */
		const uint32_t DependencyCount = 4;
		VkSubpassDependency SubpassDependencies[DependencyCount];

		// 3.1 External to Sub-pass 0
		// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;								// external sub-pass, which is the c++ code
		SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;				// Stages of the pipeline, the transition happens right after the last of the pipeline
		SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;						// Happens after memory read 

		SubpassDependencies[0].dstSubpass = 0;													// fist sub-pass
		SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// transition happens before color attachment output stage
		SubpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		SubpassDependencies[0].dependencyFlags = 0;

		// 3.2 External to Sub-pass 0
		SubpassDependencies[1].srcSubpass = 0;
		SubpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		SubpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		SubpassDependencies[1].dstSubpass = 1;
		SubpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		SubpassDependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		SubpassDependencies[1].dependencyFlags = 0;

		// 3.3 Sub-pass 1 (output color / depth) to sub-pass 2 (shader read)
		SubpassDependencies[2].srcSubpass = 1;
		SubpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Color attachment finish output before trying to read it.
		SubpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// Since it is output, which means it is write bit

		SubpassDependencies[2].dstSubpass = 2;
		SubpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// Access shader-read-only part of the two attachments on the fragment shader stage, so need to finish before this stage
		SubpassDependencies[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;						// Before the shader tries to read it
		SubpassDependencies[2].dependencyFlags = 0;

		// 3.4 sub-pass 2 to External
		// Conversion from VK_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		SubpassDependencies[3].srcSubpass = 2;
		SubpassDependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		SubpassDependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		SubpassDependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
		SubpassDependencies[3].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		SubpassDependencies[3].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		SubpassDependencies[3].dependencyFlags = 0;

		/** 4. Create Render Pass from 1. attachment descriptions, 2. sub-pass description, 3.sub-pass dependencies*/
		const uint32_t AttachmentCount = 3;
		VkAttachmentDescription Attachments[AttachmentCount] = { SwapChainColorAttachment, ColorAttachment, DepthAttachment };

		VkRenderPassCreateInfo RendePassCreateInfo = {};
		RendePassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		RendePassCreateInfo.attachmentCount = AttachmentCount;
		RendePassCreateInfo.pAttachments = Attachments;
		RendePassCreateInfo.subpassCount = SubpassCount;
		RendePassCreateInfo.pSubpasses = Subpasses;
		RendePassCreateInfo.dependencyCount = DependencyCount;
		RendePassCreateInfo.pDependencies = SubpassDependencies;

		VkResult Result = vkCreateRenderPass(MainDevice.LD, &RendePassCreateInfo, nullptr, &RenderPass);
		RESULT_CHECK(Result, "Fail to create render pass.");

	}
	void VKRenderer::CreateDescriptorSets()
	{
		// 1. Prepare DescriptorSet Info
		size_t Count = SwapChain.Images.size();
		ParticleDescriptorSet = cDescriptorSet(&MainDevice);
		DescriptorSets.resize(Count, cDescriptorSet(&MainDevice));
		InputDescriptorSets.resize(Count, cDescriptorSet(&MainDevice));
		// Create Buffers
		for (size_t i = 0; i < Count; ++i)
		{
			DescriptorSets[i].CreateBufferDescriptor(sizeof(BufferFormats::FFrame), 1, VK_SHADER_STAGE_VERTEX_BIT);
			DescriptorSets[i].CreateDynamicBufferDescriptor(sizeof(BufferFormats::FDrawCall), MAX_OBJECTS, VK_SHADER_STAGE_VERTEX_BIT);

			InputDescriptorSets[i].CreateImageBufferDescriptor(&ColorBuffers[i], VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			InputDescriptorSets[i].CreateImageBufferDescriptor(&DepthBuffers[i], VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		cTexture* ParticleTestTex = cTexture::Get(1).get();
		ParticleDescriptorSet.CreateImageBufferDescriptor(&ParticleTestTex->GetImageBuffer(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ParticleTestTex->GetImageInfo().imageLayout, ParticleTestTex->GetImageInfo().sampler);

		// 2. Create Descriptor Pool
		createDescriptorPool();
		// 3. Create Descriptor Set Layout
		for (size_t i = 0; i < DescriptorSets.size(); ++i)
		{
			// UNIFORM DESCRIPTOR SET LAYOUT
			DescriptorSets[i].CreateDescriptorSetLayout(FirstPass_vert);
			// INPUT DESCRIPTOR LAYOUT
			InputDescriptorSets[i].CreateDescriptorSetLayout(ThirdPass_frag);
		}
		// PARTICLE DESCRIPTOR LAYOUT
		ParticleDescriptorSet.CreateDescriptorSetLayout(SecondPass_frag);
		for (size_t i = 0; i < Count; ++i)
		{
			// 4. Allocate Descriptor sets
			DescriptorSets[i].AllocateDescriptorSet(DescriptorPool);
			InputDescriptorSets[i].AllocateDescriptorSet(DescriptorPool);
			// 5. Update set write info
			DescriptorSets[i].BindDescriptorWithSet();
			InputDescriptorSets[i].BindDescriptorWithSet();
		}
		ParticleDescriptorSet.AllocateDescriptorSet(SamplerDescriptorPool);
		ParticleDescriptorSet.BindDescriptorWithSet();
	}

	void VKRenderer::createPushConstantRange()
	{
		// Define push constant range, no need to create
		PushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		PushConstantRange.offset = 0;
		PushConstantRange.size = sizeof(glm::mat4);		// Size of data being pass
	}

	void VKRenderer::createGraphicsPipeline()
	{
		/** 1. Create first Pipeline */
		VkGraphicsPipelineCreateInfo PieplineCreateInfo = {};

		// === Read in SPIR-V code of shaders === 
		auto VertexShaderCode = FileIO::ReadFile("Content/Shaders/vert.spv");
		auto FragShaderCode = FileIO::ReadFile("Content/Shaders/frag.spv");

		// Build Shader Module to link to Graphics Pipeline
		FShaderModuleScopeGuard VertexShaderModule, FragmentShaderModule;
		VertexShaderModule.CreateShaderModule(MainDevice.LD, VertexShaderCode);
		FragmentShaderModule.CreateShaderModule(MainDevice.LD, FragShaderCode);

		/* 1. Shader Creation Information stage*/
		// Vertex shader
		VkPipelineShaderStageCreateInfo VSCreateInfo = Helpers::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, VertexShaderModule.ShaderModule);
		// Fragment shader
		VkPipelineShaderStageCreateInfo FSCreateInfo = Helpers::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, FragmentShaderModule.ShaderModule);

		const uint32_t ShaderStageCount = 2;
		VkPipelineShaderStageCreateInfo ShaderStages[ShaderStageCount] =
		{
			VSCreateInfo, FSCreateInfo
		};

		// === Vertex Input === 
		VkPipelineVertexInputStateCreateInfo VertexInputCreateInfo = {};

		// How the data for a single vertex (including position, color, normal, texture coordinate) is as a whole
		VkVertexInputBindingDescription VertexBindDescription = {};
		VertexBindDescription.binding = VERTEX_BUFFER_BIND_ID;					// Can bind multiple streams of data, this defines which one
		VertexBindDescription.stride = sizeof(FVertex);							// Size of of a single vertex object
		VertexBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;			// Define how to move between data after each vertex, 
																				// VK_VERTEX_INPUT_RATE_VERTEX : move to the next vertex
																				// VK_VERTEX_INPUT_RATE_INSTANCE : Move to a vertex for the next instance
		// How the data for an attribute is defined within a vertex
		const uint32_t AttrubuteDescriptionCount = 3;
		VkVertexInputAttributeDescription VertexInputAttributeDescriptions[AttrubuteDescriptionCount];

		// Position attribute
		VertexInputAttributeDescriptions[0].binding = VERTEX_BUFFER_BIND_ID;				// This binding corresponds to the layout(binding = 0, location = 0) in vertex shader, should be same as above
		VertexInputAttributeDescriptions[0].location = 0;									// This binding corresponds to the layout(binding = 0, location = 0) in vertex shader, this is a position data
		VertexInputAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;			// format of the data, defnie the size of the data, 3 * 32 bit float data
		VertexInputAttributeDescriptions[0].offset = offsetof(FVertex, Position);			// Similar stride concept, position start at 0, but the following attribute data should has offset of sizeof(glm::vec3)
		// Color attribute ...
		VertexInputAttributeDescriptions[1].binding = VERTEX_BUFFER_BIND_ID;
		VertexInputAttributeDescriptions[1].location = 1;
		VertexInputAttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		VertexInputAttributeDescriptions[1].offset = offsetof(FVertex, Color);
		// texture coordinate attribute
		VertexInputAttributeDescriptions[2].binding = VERTEX_BUFFER_BIND_ID;
		VertexInputAttributeDescriptions[2].location = 2;
		VertexInputAttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		VertexInputAttributeDescriptions[2].offset = offsetof(FVertex, TexCoord);

		VertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		VertexInputCreateInfo.pVertexBindingDescriptions = &VertexBindDescription;				// list of vertex binding description data spacing, stride info
		VertexInputCreateInfo.vertexAttributeDescriptionCount = AttrubuteDescriptionCount;
		VertexInputCreateInfo.pVertexAttributeDescriptions = VertexInputAttributeDescriptions;				// data format where to bind in shader


		// === Input Assembly ===
		VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo = {};

		InputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		InputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;			// GL_TRIANGLE, GL_LINE, GL_POINT...
		InputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;						// Allow overriding of "strip" topology to start new primitives


		// === Viewport & Scissor ===
		VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};

		// Create a viewport info struct
		VkViewport ViewPort = {};
		ViewPort.x = 0.0f;													// X start coordinate
		ViewPort.y = 0.0f;													// Y start coordinate
		ViewPort.width = static_cast<float>(SwapChain.Extent.width);		// width of viewport
		ViewPort.height = static_cast<float>(SwapChain.Extent.height);		// height of viewport
		ViewPort.minDepth = 0.0f;											// min depth, depth in vulkan is in [0.0, 1.0]
		ViewPort.maxDepth = 1.0f;											// max depth

		// Create a Scissor info struct
		VkRect2D Scissor = {};
		Scissor.offset = { 0,0 };											// Define visible region
		Scissor.extent = SwapChain.Extent;

		ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		ViewportStateCreateInfo.viewportCount = 1;
		ViewportStateCreateInfo.pViewports = &ViewPort;
		ViewportStateCreateInfo.scissorCount = 1;
		ViewportStateCreateInfo.pScissors = &Scissor;


		// === Dynamic States ===
		// Dynamic states to enable
		std::vector<VkDynamicState> DynamicStateEnables;
		DynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);			// Dynamic Viewport : Can resize in command buffer with vkCmdSetViewPort(commandBuffer, (start)0, (amount)1, &viewport);
		DynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);			// Dynamic Viewport : Can resize in command buffer with vkCmdSetScissor(commandBuffer, (start)0, (amount)1, &viewport);

		VkPipelineDynamicStateCreateInfo DynamicStateCraeteInfo;

		DynamicStateCraeteInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		DynamicStateCraeteInfo.pNext = nullptr;
		DynamicStateCraeteInfo.dynamicStateCount = static_cast<uint32_t>(DynamicStateEnables.size());
		DynamicStateCraeteInfo.pDynamicStates = DynamicStateEnables.data();
		DynamicStateCraeteInfo.flags = 0;


		// === Rasterizer ===
		VkPipelineRasterizationStateCreateInfo RasterizerCreateInfo = {};

		RasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		RasterizerCreateInfo.depthClampEnable = VK_TRUE;					// Clip behind far-plane, object behind far-plane will be rendered with depth of the far-plane
		RasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;			// If enable, all stages after Rasterizer will be discarded, not generating fragment
		RasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;			// Useful effect on wire frame effect.
		RasterizerCreateInfo.lineWidth = 1.0f;								// How thick lines should be when drawn, needs to enable other extensions
		RasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;				// Which face of a tri to cull
		RasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	// right-handed rule
		RasterizerCreateInfo.depthBiasEnable = VK_FALSE;					// Define the depth bias to fragments (good for stopping "shadow arcane")


		// === Multi-sampling ===
		VkPipelineMultisampleStateCreateInfo MSCreateInfo;

		MSCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		MSCreateInfo.pNext = nullptr;
		MSCreateInfo.sampleShadingEnable = VK_FALSE;						// Enable Multi-sampling or not
		MSCreateInfo.alphaToCoverageEnable = VK_FALSE;						// 
		MSCreateInfo.alphaToOneEnable = VK_FALSE;							//
		MSCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;			// Number of samples to use per fragment
		MSCreateInfo.pSampleMask = nullptr;
		MSCreateInfo.flags = 0;


		// === Blending ===
		// Blending decides how to blend a new color being written to a fragment, with the old value
		VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};

		// Blend attachment state (How blending is handled)
		VkPipelineColorBlendAttachmentState ColorStateAttachments = {};
		ColorStateAttachments.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // Color to apply blending to. use all channels to blend
		ColorStateAttachments.blendEnable = VK_TRUE;

		// Blending uses equation : (srcColorBlendFactor * newColor) colorBlendOp (destColorBlendFactor * oldColor)
		ColorStateAttachments.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		ColorStateAttachments.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		ColorStateAttachments.colorBlendOp = VK_BLEND_OP_ADD;						// additive blending in PS.
		// Blending color equation : (newColorAlpha * NewColor) + ((1 - newColorAlpha) * OldColor)

		ColorStateAttachments.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		ColorStateAttachments.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		ColorStateAttachments.alphaBlendOp = VK_BLEND_OP_ADD;
		// Blending alpha equation (1 * newAlpha) + (0 * oldAlpha)

		ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;					// Alternative to calculation is to use logical operations
		//ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
		ColorBlendStateCreateInfo.attachmentCount = 1;
		ColorBlendStateCreateInfo.pAttachments = &ColorStateAttachments;


		// === Pipeline layout ===
		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};

		const uint32_t SetLayoutCount = 2;
		VkDescriptorSetLayout Layouts[SetLayoutCount] = { DescriptorSets[0].GetDescriptorSetLayout(), cDescriptorSet::GetDescriptorSetLayout(FirstPass_frag) };

		PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutCreateInfo.setLayoutCount = SetLayoutCount;
		PipelineLayoutCreateInfo.pSetLayouts = Layouts;
		PipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		PipelineLayoutCreateInfo.pPushConstantRanges = &PushConstantRange;

		// Create Pipeline layout
		VkResult Result = vkCreatePipelineLayout(MainDevice.LD, &PipelineLayoutCreateInfo, nullptr, &PipelineLayout);
		RESULT_CHECK(Result, "Fail to create Pipeline Layout.");

		// === Depth Stencil testing ===
		// Set up depth stencil testing
		VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo = {};
		DepthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		DepthStencilCreateInfo.depthTestEnable = VK_TRUE;
		DepthStencilCreateInfo.depthWriteEnable = VK_TRUE;
		DepthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;		// Compare operation that allows on overwrite
		DepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;		// Depth bounce test: does the depth value exist between two bounces
		DepthStencilCreateInfo.stencilTestEnable = VK_FALSE;			// Don't need stencil test

		// === Graphic Pipeline Creation ===
		VkGraphicsPipelineCreateInfo PipelineCreateInfo = {};

		PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		PipelineCreateInfo.stageCount = ShaderStageCount;													// Shader stage count
		PipelineCreateInfo.pStages = ShaderStages;											// Shader create info
		PipelineCreateInfo.pVertexInputState = &VertexInputCreateInfo;
		PipelineCreateInfo.pInputAssemblyState = &InputAssemblyCreateInfo;
		PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;
		PipelineCreateInfo.pDynamicState = nullptr; //&DynamicStateCraeteInfo;
		PipelineCreateInfo.pRasterizationState = &RasterizerCreateInfo;
		PipelineCreateInfo.pMultisampleState = &MSCreateInfo;
		PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
		PipelineCreateInfo.pDepthStencilState = &DepthStencilCreateInfo;
		PipelineCreateInfo.layout = PipelineLayout;
		PipelineCreateInfo.renderPass = RenderPass;											// RenderPass description the pipeline is compatible with
		PipelineCreateInfo.subpass = 0;														// Sub-pass of render pass to use with pipeline

		// Pipeline Derivatives: Can create multiple pipelines that derive from one for optimization
		PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	//PipelineCreateInfo.basePipelineHandle = OldPipeline;	
		PipelineCreateInfo.basePipelineIndex = -1;

		// PipelineCache can save the cache when the next time create a pipeline
		Result = vkCreateGraphicsPipelines(MainDevice.LD, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &GraphicPipeline);
		RESULT_CHECK(Result, "Fail to create Graphics Pipelines.");
		/** 2. Create second Pipeline: Particle rendering*/
		{
			// === Read in SPIR-V code of shaders === 
			auto VertexShaderCode1 = FileIO::ReadFile("Content/Shaders/particle/particle.vert.spv");
			auto FragShaderCode1 = FileIO::ReadFile("Content/Shaders/particle/particle.frag.spv");

			// Build Shader Module to link to Graphics Pipeline
			FShaderModuleScopeGuard VertexShaderModule1, FragmentShaderModule1;
			VertexShaderModule1.CreateShaderModule(MainDevice.LD, VertexShaderCode1);
			FragmentShaderModule1.CreateShaderModule(MainDevice.LD, FragShaderCode1);

			/* reuse create info from the first render pass*/
			// Vertex shader
			VSCreateInfo.module = VertexShaderModule1.ShaderModule;
			FSCreateInfo.module = FragmentShaderModule1.ShaderModule;

			VkPipelineShaderStageCreateInfo ShaderStages1[ShaderStageCount] =
			{
				VSCreateInfo, FSCreateInfo
			};

			VkVertexInputBindingDescription ParticleInstanceInputBindingDescription = {};
			ParticleInstanceInputBindingDescription.binding = INSTANCE_BUFFER_BIND_ID;
			ParticleInstanceInputBindingDescription.stride = sizeof(BufferFormats::FParticle);		// Position
			ParticleInstanceInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;		// Want to use instance draw to draw the particle

			const uint32_t ParticleInputAttributeDescriptionCount = 6;
			VkVertexInputAttributeDescription ParticleInputAttributeDescriptions[ParticleInputAttributeDescriptionCount];
			ParticleInputAttributeDescriptions[0] = VertexInputAttributeDescriptions[0];
			ParticleInputAttributeDescriptions[1] = VertexInputAttributeDescriptions[1];
			ParticleInputAttributeDescriptions[2] = VertexInputAttributeDescriptions[2];

			ParticleInputAttributeDescriptions[3].binding = INSTANCE_BUFFER_BIND_ID;
			ParticleInputAttributeDescriptions[3].location = 3;
			ParticleInputAttributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;				// including elapsed life time a Pos.w
			ParticleInputAttributeDescriptions[3].offset = offsetof(BufferFormats::FParticle, Pos);

			ParticleInputAttributeDescriptions[4].binding = INSTANCE_BUFFER_BIND_ID;
			ParticleInputAttributeDescriptions[4].location = 4;
			ParticleInputAttributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;				// including life time in Vel.w
			ParticleInputAttributeDescriptions[4].offset = offsetof(BufferFormats::FParticle, Vel);

			ParticleInputAttributeDescriptions[5].binding = INSTANCE_BUFFER_BIND_ID;
			ParticleInputAttributeDescriptions[5].location = 5;
			ParticleInputAttributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;				
			ParticleInputAttributeDescriptions[5].offset = offsetof(BufferFormats::FParticle, ColorOverlay);

			const uint32_t BindDescriptionCount = 2;
			VkVertexInputBindingDescription BindingDescriptions[BindDescriptionCount] = { VertexBindDescription, ParticleInstanceInputBindingDescription };
			// particle vertex data
			VertexInputCreateInfo.vertexBindingDescriptionCount = BindDescriptionCount;
			VertexInputCreateInfo.pVertexBindingDescriptions = BindingDescriptions;
			VertexInputCreateInfo.vertexAttributeDescriptionCount = ParticleInputAttributeDescriptionCount;
			VertexInputCreateInfo.pVertexAttributeDescriptions = ParticleInputAttributeDescriptions;

			// No longer drawing point, instead is drawing bill board
			//InputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			
			// Disable DepthTest and DepthWrite for particles
			DepthStencilCreateInfo.depthTestEnable = VK_FALSE;
			DepthStencilCreateInfo.depthWriteEnable = VK_FALSE;
			
			// Change blending stage
			VkPipelineColorBlendStateCreateInfo ParticleColorBlendStateCreateInfo = {};

			// Blend attachment state (How blending is handled)
			VkPipelineColorBlendAttachmentState ParticleColorStateAttachments = {};
			ParticleColorStateAttachments.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // Color to apply blending to. use all channels to blend
			ParticleColorStateAttachments.blendEnable = VK_TRUE;

			// Pre-multiplied alpha
			ParticleColorStateAttachments.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			ParticleColorStateAttachments.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			ParticleColorStateAttachments.colorBlendOp = VK_BLEND_OP_ADD;						// additive blending in PS.
			// Blending color equation : (newColorAlpha * NewColor) + ((1 - newColorAlpha) * OldColor)

			ParticleColorStateAttachments.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			ParticleColorStateAttachments.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			ParticleColorStateAttachments.alphaBlendOp = VK_BLEND_OP_ADD;
			// Blending alpha equation (1 * newAlpha) + (1 * oldAlpha)

			ParticleColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			ParticleColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;					// Alternative to calculation is to use logical operations
			//ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
			ParticleColorBlendStateCreateInfo.attachmentCount = 1;
			ParticleColorBlendStateCreateInfo.pAttachments = &ParticleColorStateAttachments;

			// Create Another pipeline layout
			const uint32_t ParticleSetLayoutCount = 2;
			VkDescriptorSetLayout ParticlePassLayouts[ParticleSetLayoutCount] = { DescriptorSets[0].GetDescriptorSetLayout(), ParticleDescriptorSet.GetDescriptorSetLayout() };

			VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo1 = {};
			PipelineLayoutCreateInfo1.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			PipelineLayoutCreateInfo1.setLayoutCount = ParticleSetLayoutCount;
			PipelineLayoutCreateInfo1.pSetLayouts = ParticlePassLayouts;
			PipelineLayoutCreateInfo1.pushConstantRangeCount = 0;
			PipelineLayoutCreateInfo1.pPushConstantRanges = nullptr;

			Result = vkCreatePipelineLayout(MainDevice.LD, &PipelineLayoutCreateInfo1, nullptr, &RenderParticlePipelineLayout);
			RESULT_CHECK(Result, "Fail to create the second pipeline layout");

			// Update PipelineCraeteInfo according to the previous changes
			PipelineCreateInfo.pStages = ShaderStages1;
			PipelineCreateInfo.layout = RenderParticlePipelineLayout;
			PipelineCreateInfo.subpass = 1;	// Which sub-pass this pipeline is in 
			PipelineCreateInfo.pColorBlendState = &ParticleColorBlendStateCreateInfo;
			// Create the second pipeline 
			Result = vkCreateGraphicsPipelines(MainDevice.LD, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &RenderParticlePipeline);
			RESULT_CHECK(Result, "Fail to create the second Graphics Pipelines.");
		}
		/** 3. Create third Pipeline*/
		{
			// === Read in SPIR-V code of shaders === 
			auto VertexShaderCode2 = FileIO::ReadFile("Content/Shaders/bigTriangle.spv");
			auto FragShaderCode2 = FileIO::ReadFile("Content/Shaders/second.spv");

			// Build Shader Module to link to Graphics Pipeline
			FShaderModuleScopeGuard VertexShaderModule2, FragmentShaderModule2;
			VertexShaderModule2.CreateShaderModule(MainDevice.LD, VertexShaderCode2);
			FragmentShaderModule2.CreateShaderModule(MainDevice.LD, FragShaderCode2);

			/* reuse create info from the first render pass*/
			// Vertex shader
			VSCreateInfo.module = VertexShaderModule2.ShaderModule;
			FSCreateInfo.module = FragmentShaderModule2.ShaderModule;

			VkPipelineShaderStageCreateInfo ShaderStages2[ShaderStageCount] =
			{
				VSCreateInfo, FSCreateInfo
			};

			// No vertex data for the third pass
			VertexInputCreateInfo.vertexBindingDescriptionCount = 0;
			VertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
			VertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
			VertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

			// disable depth / stencil testing
			DepthStencilCreateInfo.depthWriteEnable = VK_FALSE;

			// Change back to Triangle list
			InputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			// Create Another pipeline layout
			VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo2 = {};
			PipelineLayoutCreateInfo2.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			PipelineLayoutCreateInfo2.setLayoutCount = 1;
			PipelineLayoutCreateInfo2.pSetLayouts = &InputDescriptorSets[0].GetDescriptorSetLayout();
			PipelineLayoutCreateInfo2.pushConstantRangeCount = 0;
			PipelineLayoutCreateInfo2.pPushConstantRanges = nullptr;

			Result = vkCreatePipelineLayout(MainDevice.LD, &PipelineLayoutCreateInfo2, nullptr, &PostProcessPipelineLayout);
			RESULT_CHECK(Result, "Fail to create the second pipeline layout");

			// Update PipelineCraeteInfo according to the previous changes
			PipelineCreateInfo.pStages = ShaderStages2;
			PipelineCreateInfo.layout = PostProcessPipelineLayout;
			PipelineCreateInfo.subpass = 2;	// Which sub-pass this pipeline is in 
			PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
			// Create the third pipeline 
			Result = vkCreateGraphicsPipelines(MainDevice.LD, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &PostProcessPipeline);
			RESULT_CHECK(Result, "Fail to create the third Graphics Pipelines.");
		}

	}

	void VKRenderer::recreateSwapChain()
	{
		vkDeviceWaitIdle(MainDevice.LD);

		//pCompute->recreateSwapChain();

		cleanupSwapChain();

		createSwapChain();
		createRenderPass();
		createGraphicsPipeline();
		createFrameBuffer();
		createCommandBuffers();
	}

	void VKRenderer::cleanupSwapChain()
	{
		for (auto& FrameBuffer : SwapChainFramebuffers)
		{
			vkDestroyFramebuffer(MainDevice.LD, FrameBuffer, nullptr);
		}

		vkFreeCommandBuffers(MainDevice.LD, MainDevice.GraphicsCommandPool, static_cast<uint32_t>(CommandBuffers.size()), CommandBuffers.data());

		// Destroy pipelines first and then destroy render pass
		vkDestroyPipeline(MainDevice.LD, PostProcessPipeline, nullptr);
		vkDestroyPipelineLayout(MainDevice.LD, PostProcessPipelineLayout, nullptr);

		vkDestroyPipeline(MainDevice.LD, RenderParticlePipeline, nullptr);
		vkDestroyPipelineLayout(MainDevice.LD, RenderParticlePipelineLayout, nullptr);

		vkDestroyPipeline(MainDevice.LD, GraphicPipeline, nullptr);
		vkDestroyPipelineLayout(MainDevice.LD, PipelineLayout, nullptr);

		vkDestroyRenderPass(MainDevice.LD, RenderPass, nullptr);

		for (auto & Image : SwapChain.Images)
		{
			vkDestroyImageView(MainDevice.LD, Image.ImgView, nullptr);
		}
		vkDestroySwapchainKHR(MainDevice.LD, SwapChain.SwapChain, nullptr);

	}

	void VKRenderer::createFrameBufferImage()
	{
		DepthBuffers.resize(SwapChain.Images.size());
		ColorBuffers.resize(SwapChain.Images.size());

		VkFormat DepthFormat = chooseSupportedFormat(
			{
				VK_FORMAT_D32_SFLOAT_S8_UINT,	// 32-bit Depth and 8-bit Stencil buffer
				VK_FORMAT_D32_SFLOAT,			// If stencil buffer is not available
				VK_FORMAT_D24_UNORM_S8_UINT,	// 24 unsigned normalized Depth	
			},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

		VkFormat ColorFormat = chooseSupportedFormat(
			{
				VK_FORMAT_R8G8B8A8_UNORM
			},
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		for (size_t i = 0; i < SwapChain.Images.size(); ++i)
		{
			if (!DepthBuffers[i].init(&MainDevice, SwapChain.Extent.width, SwapChain.Extent.height, DepthFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |	// Use as depth / stencil attachment output
				VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT			// Also use as input in the next sub-pass
				, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_DEPTH_BIT))
			{
				throw std::runtime_error("Fail to create depth buffer image");
				return;
			}

			if (!ColorBuffers[i].init(&MainDevice, SwapChain.Extent.width, SwapChain.Extent.height, ColorFormat,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |		// Use as color attachment output
				VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT			// Also use as input in the next sub-pass
				, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT))
			{
				throw std::runtime_error("Fail to create color buffer image");
				return;
			}
		}
	}

	void VKRenderer::createFrameBuffer()
	{
		// Resize frame buffer count to equal swap chain image count
		SwapChainFramebuffers.resize(SwapChain.Images.size());

		// Create a frame buffer for each swap chain image
		for (size_t i = 0; i < SwapChainFramebuffers.size(); ++i)
		{
			const uint32_t AttachmentCount = 3;
			VkImageView Attachments[AttachmentCount] =
			{
				SwapChain.Images[i].ImgView,
				ColorBuffers[i].GetImageView(),
				DepthBuffers[i].GetImageView()
			};

			VkFramebufferCreateInfo FramebufferCreateInfo = {};
			FramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			FramebufferCreateInfo.renderPass = RenderPass;								// Render Pass layout the frame buffer will be used with
			FramebufferCreateInfo.attachmentCount = AttachmentCount;
			FramebufferCreateInfo.pAttachments = Attachments;							// List of attachments (1:1 with Render pass)
			FramebufferCreateInfo.width = SwapChain.Extent.width;
			FramebufferCreateInfo.height = SwapChain.Extent.height;
			FramebufferCreateInfo.layers = 1;											// Frame buffer layers

			// Frame buffer is 1 to 1 connected to ImageView in SwapChain now
			VkResult Result = vkCreateFramebuffer(MainDevice.LD, &FramebufferCreateInfo, nullptr, &SwapChainFramebuffers[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create Frame buffer[%d].", i);
		}
	}

	void VKRenderer::createCommandPool()
	{
		VkCommandPoolCreateInfo CommandPoolCreateInfo;
		CommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		CommandPoolCreateInfo.pNext = nullptr;
		CommandPoolCreateInfo.queueFamilyIndex = MainDevice.QueueFamilyIndices.graphicFamily;					// Queue family type that buffers from this command pool will use
		CommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;	// Allow reset so that we can re-record in run-time
																						// VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT makes vkBeginCommandBuffer(...) dump all previous commands

		// Create a Graphics Queue Family Command Pool
		VkResult Result = vkCreateCommandPool(MainDevice.LD, &CommandPoolCreateInfo, nullptr, &MainDevice.GraphicsCommandPool);
		RESULT_CHECK(Result, "Fail to create a command pool.");
	}

	void VKRenderer::createCommandBuffers()
	{
		//Resize command buffer count to have one for each framebuffer
		CommandBuffers.resize(SwapChainFramebuffers.size());

		VkCommandBufferAllocateInfo cbAllocInfo = {};										// Memory exists already, only get it from the pool
		cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbAllocInfo.commandPool = MainDevice.GraphicsCommandPool;							// Only works for graphics command pool
		cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;								// VK_COMMAND_BUFFER_LEVEL_PRIMARY: Submitted directly to queue, can not be called by another command buffer
																							// VK_COMMAND_BUFFER_LEVEL_SECONDARY: Command buffer within a command buffer e.g. VkCmdExecuteCommands(another command buffer)
		cbAllocInfo.commandBufferCount = static_cast<uint32_t>(CommandBuffers.size());		// Allows allocating multiple command buffers at the same time

		VkResult Result = vkAllocateCommandBuffers(MainDevice.LD, &cbAllocInfo, CommandBuffers.data());		// Don't need a custom allocation function
		RESULT_CHECK(Result, "Fail to allocate command buffers.");

		// Record the command buffer and call this buffer once and once again.
	}

	void VKRenderer::createSynchronization()
	{
		OnGraphicFinished.resize(MAX_FRAME_DRAWS);
		OnImageAvailables.resize(MAX_FRAME_DRAWS);
		OnRenderFinisheds.resize(MAX_FRAME_DRAWS);
		DrawFences.resize(MAX_FRAME_DRAWS);

		// Semaphore creation information
		VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
		SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		// Fence creation information
		VkFenceCreateInfo FenceCreateInfo = {};
		FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
		{
			VkResult Result = vkCreateSemaphore(MainDevice.LD, &SemaphoreCreateInfo, nullptr, &OnGraphicFinished[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create OnGraphicFinished[%d] Semaphore", i);

			Result = vkCreateSemaphore(MainDevice.LD, &SemaphoreCreateInfo, nullptr, &OnImageAvailables[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create OnImageAvailables[%d] Semaphore", i);

			Result = vkCreateSemaphore(MainDevice.LD, &SemaphoreCreateInfo, nullptr, &OnRenderFinisheds[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create OnRenderFinisheds[%d] Semaphore", i);

			Result = vkCreateFence(MainDevice.LD, &FenceCreateInfo, nullptr, &DrawFences[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create DrawFences[%d] Fence", i);
		}
	}

	void VKRenderer::createDescriptorPool()
	{
		// Type of descriptors + how many DESCRIPTORS, not DESCRIPTOR Sets (combined makes the pool size)
		// in order to enable imgui, needs a larger descriptor pool
		const uint32_t DescriptorTypeCount = 11;
		const uint32_t MaxDescriptorsPerType = 1000;
		VkDescriptorPoolSize PoolSize[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, MaxDescriptorsPerType },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, MaxDescriptorsPerType }
		};

		VkDescriptorPoolCreateInfo PoolCreateInfo = {};
		PoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		PoolCreateInfo.maxSets = DescriptorTypeCount * MaxDescriptorsPerType;
		PoolCreateInfo.poolSizeCount = DescriptorTypeCount;
		PoolCreateInfo.pPoolSizes = PoolSize;

		VkResult Result = vkCreateDescriptorPool(MainDevice.LD, &PoolCreateInfo, nullptr, &DescriptorPool);
		RESULT_CHECK(Result, "Failed to create a Descriptor Pool");

		// CREATE SAMPLER DESCRIPTOR POOL
		{
			VkDescriptorPoolSize SamplerPoolSize = {};
			SamplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			SamplerPoolSize.descriptorCount = MAX_OBJECTS * 2;		// Not optimal setup, one image binds to one descriptor

			VkDescriptorPoolCreateInfo SamplerPoolCreateInfo = {};
			SamplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			SamplerPoolCreateInfo.maxSets = MAX_OBJECTS;			// One object binds to one descriptor sets
			SamplerPoolCreateInfo.poolSizeCount = 1;
			SamplerPoolCreateInfo.pPoolSizes = &SamplerPoolSize;

			Result = vkCreateDescriptorPool(MainDevice.LD, &SamplerPoolCreateInfo, nullptr, &SamplerDescriptorPool);
			RESULT_CHECK(Result, "Failed to create a Sampler Descriptor Pool");
		}
	}

	void VKRenderer::updateUniformBuffers()
	{
		int idx = SwapChain.ImageIndex;
		// Copy Frame data
		if (cDescriptor_Buffer* Buffer = DescriptorSets[idx].GetDescriptorAt<cDescriptor_Buffer>(0))
		{
			Buffer->UpdateBufferData(&GetCurrentCamera()->GetFrameData());
		}

		if (cDescriptor_DynamicBuffer* DBuffer = DescriptorSets[idx].GetDescriptorAt<cDescriptor_DynamicBuffer>(1))
		{
			using namespace BufferFormats;
			// Update model data to pDrawcallTransferSpace
			for (size_t i = 0; i < RenderList.size(); ++i)
			{
				FDrawCall* Drawcall = reinterpret_cast<FDrawCall*>(reinterpret_cast<uint64_t>(DBuffer->GetAllocatedMemory()) + (i *DBuffer->GetSlotSize()));
				*Drawcall = RenderList[i]->Transform.M();
			}
			// Particle is drawn after all render objects
			FDrawCall* ParticleDrawcall = reinterpret_cast<FDrawCall*>(reinterpret_cast<uint64_t>(DBuffer->GetAllocatedMemory()) + (RenderList.size() *DBuffer->GetSlotSize()));
			*ParticleDrawcall = pCompute->Emitter.Transform.M();

			// Copy Model data RenderList.Size() + Emitter.Size()
			// Reuse void* Data
			size_t DBufferSize = static_cast<size_t>(DBuffer->GetSlotSize()) * (RenderList.size() + 1);
			DBuffer->UpdatePartialData(DBuffer->GetAllocatedMemory(), 0, DBufferSize);
		}

	}

	bool VKRenderer::checkInstanceExtensionSupport(const char** checkExtentions, int extensionCount)
	{
		// Need to get number of extensions to create array of correct size to hold extensions
		uint32_t ExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);

		printf("Instance Extension Count: %i\n", ExtensionCount);
		if (ExtensionCount == 0)
		{
			return false;
		}

		std::vector<VkExtensionProperties> extensions(ExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, extensions.data());

		// Check if given extensions are available
		for (int i = 0; i < extensionCount; ++i)
		{
			bool hasExtension = false;
			for (const auto& extension : extensions)
			{
				if (strcmp(checkExtentions[i], extension.extensionName))
				{
					hasExtension = true;
					break;
				}
			}
			if (!hasExtension)
			{
				return false;
			}
		}
		return true;
	}

	bool VKRenderer::checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : ValidationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	bool VKRenderer::checkDeviceExtensionSupport(const VkPhysicalDevice& device)
	{
		uint32_t ExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &ExtensionCount, nullptr);

		printf("Physical Device Extension Count: %i\n", ExtensionCount);
		if (ExtensionCount == 0)
		{
			return false;
		}

		std::vector<VkExtensionProperties> Extensions(ExtensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &ExtensionCount, Extensions.data());

		for (const auto& deviceExtension : DeviceExtensions)
		{
			bool hasExtension = false;
			for (const auto & extension : Extensions)
			{
				if (strcmp(deviceExtension, extension.extensionName))
				{
					hasExtension = true;
					break;
				}
			}
			if (!hasExtension)
			{
				return false;
			}
		}
		return true;
	}

	bool VKRenderer::checkDeviceSuitable(const VkPhysicalDevice& device)
	{
		getQueueFamilies(device);
		// Information about what the device supports (geo shader, tess shader, wide lines, etc)
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		if (!MainDevice.QueueFamilyIndices.IsValid())
		{
			return false;
		}
		if (!deviceFeatures.samplerAnisotropy)
		{
			return false;
		}
		if (!checkDeviceExtensionSupport(device))
		{
			return false;
		}
		getSwapChainDetail(device);
		if (!SwapChainDetail.IsValid())
		{
			return false;
		}

		return true;
	}

	VkFormat VKRenderer::chooseSupportedFormat(const std::vector<VkFormat>& Formats, VkImageTiling Tiling, VkFormatFeatureFlags FeatureFlags)
	{
		// Order of formats matters
		for (VkFormat Format : Formats)
		{
			// Get Properties for given format on this device
			VkFormatProperties Properties;
			vkGetPhysicalDeviceFormatProperties(MainDevice.PD, Format, &Properties);

			// Depending on tiling choice
			if (Tiling == VK_IMAGE_TILING_LINEAR && ((Properties.linearTilingFeatures & FeatureFlags) == FeatureFlags))
			{
				return Format;
			}
			else if (Tiling == VK_IMAGE_TILING_OPTIMAL && ((Properties.optimalTilingFeatures& FeatureFlags) == FeatureFlags))
			{
				return Format;
			}
		}
		throw std::runtime_error("Fail to find a matching format!");
	}

	bool VKRenderer::CreateModel(const std::string& ifileName, std::shared_ptr<cModel>& oModel)
	{
		// Import model "scene"
		Assimp::Importer Importer;
		std::string FileLoc = "Content/Models/" + ifileName;
		const aiScene* scene = Importer.ReadFile(FileLoc, aiProcess_Triangulate
			| aiProcess_FlipUVs
			| aiProcess_GenSmoothNormals
			| aiProcess_JoinIdenticalVertices
			| aiProcess_CalcTangentSpace
			| aiProcess_GenBoundingBoxes);

		if (!scene)
		{
			throw std::runtime_error("Fail to load model! (" + FileLoc + ")");
			return false;
		}

		// get vector of all materials with 1:1 ID placement
		std::vector<std::string> TextureNames = cModel::LoadMaterials(scene);

		// Conversion from the materials list IDs to our Descriptor Array IDs
		std::vector<int> MatToTex(TextureNames.size(), 0);

		// Loop over texture names and create texture for them
		for (size_t i = 0; i < MatToTex.size(); ++i)
		{
			// texture 0 will be reserved for default texture
			if (TextureNames[i].empty())
			{
				MatToTex[i] = 0;
			}
			else
			{
				auto newTex = cTexture::Load(TextureNames[i], MainDevice, VK_FORMAT_R8G8B8A8_UNORM);
				// Set value to index of new texture
				MatToTex[i] = newTex->GetID();
			}
		}

		std::vector<std::shared_ptr<cMesh>> Meshes = cModel::LoadNode(ifileName, MainDevice, MainDevice.graphicQueue, MainDevice.GraphicsCommandPool, scene->mRootNode, scene, MatToTex);
		for (auto& Mesh : Meshes)
		{
			if (Mesh.get())
			{
				Mesh->CreateDescriptorSet(SamplerDescriptorPool);
			}
		}
		oModel = std::make_shared<cModel>(Meshes);

		return true;
	}

	void VKRenderer::recordCommands()
	{
		VkCommandBuffer& CB = CommandBuffers[SwapChain.ImageIndex];
		// Begin info can be the same
		VkCommandBufferBeginInfo BufferBeginInfo = {};
		BufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//BufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;		// Buffer can be resubmitted when it has already been submitted and is awaiting execution.

		// Information about how to begin a render pass (only needed for graphical applications)
		VkRenderPassBeginInfo RenderPassBeginInfo = {};
		RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		RenderPassBeginInfo.renderPass = RenderPass;								// Render pass to begin
		RenderPassBeginInfo.renderArea.offset = { 0,0 };							// Start point of render pass in pixels
		RenderPassBeginInfo.renderArea.extent = SwapChain.Extent;					// Size of region to run render pass on starting at offset

		const uint32_t ClearColorCount = 3;
		VkClearValue ClearValues[ClearColorCount] = {};
		ClearValues[0].color = { 0.0f, 0.0f, 0.0f, 0.0f };							// SwapChain image clear color, doesn't make any difference if the image is drawn properly
		ClearValues[1].color = { 0.0f, 0.0f, 0.0f, 1.0f };							// Color attachment clear value
		ClearValues[2].depthStencil.depth = 1.0f;									// Depth attachment clear value

		RenderPassBeginInfo.pClearValues = ClearValues;								// List of clear values 
		RenderPassBeginInfo.clearValueCount = ClearColorCount;

		RenderPassBeginInfo.framebuffer = SwapChainFramebuffers[SwapChain.ImageIndex];

		// Start recording commands to command buffer
		VkResult Result = vkBeginCommandBuffer(CB, &BufferBeginInfo);
		RESULT_CHECK_ARGS(Result, "Fail to start recording a command buffer[%d]", SwapChain.ImageIndex);

		const int& GraphicFamilyIndex = MainDevice.QueueFamilyIndices.graphicFamily;
		const int& ComputeFamilyIndex = MainDevice.QueueFamilyIndices.computeFamily;
		const cBuffer& StorageBuffer = pCompute->ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(0)->GetBuffer();

		/** Record part */
		// Acquire barrier
		if (pCompute->needSynchronization())
		{
			VkBufferMemoryBarrier BufferBarrier = {};
			BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			BufferBarrier.srcAccessMask = 0;
			BufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			// Transfer ownership from compute queue to graphic queue
			BufferBarrier.srcQueueFamilyIndex = ComputeFamilyIndex;
			BufferBarrier.dstQueueFamilyIndex = GraphicFamilyIndex;
			BufferBarrier.buffer = StorageBuffer.GetvkBuffer();
			BufferBarrier.offset = 0;
			BufferBarrier.size = StorageBuffer.BufferSize();

			// Block the vertex input stage until compute shader has finished
			vkCmdPipelineBarrier(CB,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,							// No Dependency flag
				0, nullptr,					// Not a Memory barrier
				1, &BufferBarrier,			// A Buffer memory Barrier
				0, nullptr);				// Not a Image memory barrier
		}

		// Begin first Render Pass
		vkCmdBeginRenderPass(CB, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Start the first sub-pass
		// Bind Pipeline to be used in render pass
		vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicPipeline);

		// Draw all models in the render list
		for (size_t j = 0; j < RenderList.size(); ++j)
		{

			// Push constant to given shader stage directly (No Buffer)
			glm::mat4 MVP = GetCurrentCamera()->GetFrameData().PVMatrix * RenderList[j]->Transform.M();
			vkCmdPushConstants(CB, PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
				0,
				sizeof(glm::mat4),					// Size of data being pushed
				&MVP);								// Actual data being pushed

			// Draw all meshes in one model
			for (size_t k = 0; k < RenderList[j]->GetMeshCount(); ++k)
			{
				auto Mesh = RenderList[j]->GetMesh(k);
				VkBuffer VertexBuffers[] = { Mesh->GetVertexBuffer() };			// Buffers to bind
				VkDeviceSize Offsets[] = { 0 };												// Offsets into buffers being bound

				// Bind vertex data
				vkCmdBindVertexBuffers(CB, VERTEX_BUFFER_BIND_ID, 1, VertexBuffers, Offsets);	// Command to bind vertex buffer for drawing with

				// Only one index buffer is allowed, it handles all vertex buffer's index, uint32 type is more than enough for the index count
				vkCmdBindIndexBuffer(CB, Mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				// Dynamic Offset Amount
				uint32_t DynamicOffset = static_cast<uint32_t>(DescriptorSets[SwapChain.ImageIndex].GetDescriptorAt<cDescriptor_DynamicBuffer>(1)->GetSlotSize()) * j;

				const uint32_t DescriptorSetCount = 2;
				// Two descriptor sets
				VkDescriptorSet DescriptorSetGroup[] = { DescriptorSets[SwapChain.ImageIndex].GetDescriptorSet(), Mesh->GetDescriptorSet() };

				// Bind Descriptor sets for Projection / View / Model matrix
				vkCmdBindDescriptorSets(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout,
					0, DescriptorSetCount,						// One descriptor for each draw
					DescriptorSetGroup,
					1, &DynamicOffset							// Dynamic offsets
				);
				/*
				vkCmdDraw(CB[i],
					Mesh->GetVertexCount(),	// vertexCount, indicates how many times the pipeline will call
					1,						// For drawing same object multiple times
					0, 0);*/

					// Execute pipeline, Index draw
				vkCmdDrawIndexed(CB, Mesh->GetIndexCount(), 1, 0, 0, 0);
			}

		}
		// Start the second sub-pass
		{
			vkCmdNextSubpass(CB, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, RenderParticlePipeline);

			std::shared_ptr<cMesh> QuadMesh = GQuadModel->GetMesh(0);
			VkDeviceSize Offsets[] = { 0 };
			// Bind vertex buffer
			vkCmdBindVertexBuffers(CB, VERTEX_BUFFER_BIND_ID, 1, &QuadMesh->GetVertexBuffer(), Offsets);

			// Bind instance data buffer as a vertex buffer
			vkCmdBindVertexBuffers(CB, INSTANCE_BUFFER_BIND_ID, 1, &pCompute->GetStorageBuffer().GetvkBuffer(), Offsets);

			// Bind index buffer
			vkCmdBindIndexBuffer(CB, QuadMesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

			// Particle is drawn after all Model, so the offset should be RenderList.size() * Length
			uint32_t ParticleDynamicOffset = static_cast<uint32_t>(DescriptorSets[SwapChain.ImageIndex].GetDescriptorAt<cDescriptor_DynamicBuffer>(1)->GetSlotSize()) * RenderList.size();

			const uint32_t DescriptorSetCount = 2;
			// Two descriptor sets
			VkDescriptorSet DescriptorSetGroup[] = { DescriptorSets[SwapChain.ImageIndex].GetDescriptorSet(), ParticleDescriptorSet.GetDescriptorSet() };

			vkCmdBindDescriptorSets(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, RenderParticlePipelineLayout,
				0, DescriptorSetCount, DescriptorSetGroup,
				1, &ParticleDynamicOffset);	// no dynamic offset because no model matrix

			// draw the quad with multiple instance
			vkCmdDrawIndexed(CB, QuadMesh->GetIndexCount(), Particle_Count, 0, 0, 0);
		}
		// Start the third sub-pass
		{
			vkCmdNextSubpass(CB, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PostProcessPipeline);

			pCompute->ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(1)->UpdateBufferData(&pCompute->ParticleSupportData);
			// No need to bind vertex buffer or index buffer
			vkCmdBindDescriptorSets(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, PostProcessPipelineLayout,
				0, 1, &InputDescriptorSets[SwapChain.ImageIndex].GetDescriptorSet(),
				0, nullptr);	// no dynamic offset
			// Draw 3 vertex (1 triangle) only 
			vkCmdDraw(CB, 3, 1, 0, 0);
		}
		// End Render Pass
		vkCmdEndRenderPass(CB);
		
		// Begin second (imgui) render pass
		// Rendering
		ImGui::Render();
		ImDrawData* draw_data = ImGui::GetDrawData();
		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		wd->FrameIndex = SwapChain.ImageIndex;
		ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
		{
			VkRenderPassBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			info.renderPass = wd->RenderPass;
			info.framebuffer = fd->Framebuffer;
			info.renderArea.extent.width = wd->Width;
			info.renderArea.extent.height = wd->Height;
			info.clearValueCount = 1;
			info.pClearValues = &wd->ClearValue;
			vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
		}

		// Record dear imgui primitives into command buffer
		ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);
		// End imgui render pass
		vkCmdEndRenderPass(fd->CommandBuffer);

		// Release barrier
		if (pCompute->needSynchronization())
		{
			VkBufferMemoryBarrier BufferBarrier = {};
			BufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			// The storage buffer is used as vertex input
			BufferBarrier.srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			BufferBarrier.dstAccessMask = 0;
			// Transfer ownership from graphic queue to compute queue
			BufferBarrier.srcQueueFamilyIndex = GraphicFamilyIndex;
			BufferBarrier.dstQueueFamilyIndex = ComputeFamilyIndex;
			BufferBarrier.buffer = StorageBuffer.GetvkBuffer();
			BufferBarrier.offset = 0;
			BufferBarrier.size = StorageBuffer.BufferSize();

			// Block the compute shader until vertex shader finished reading the storage buffer
			vkCmdPipelineBarrier(CB,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,							// No Dependency flag
				0, nullptr,					// Not a Memory barrier
				1, &BufferBarrier,			// A Buffer memory Barrier
				0, nullptr);				// Not a Image memory barrier
		}

		Result = vkEndCommandBuffer(CB);
		RESULT_CHECK_ARGS(Result, "Fail to stop recording a command buffer[%d]", SwapChain.ImageIndex);
	}


	void VKRenderer::getQueueFamilies(const VkPhysicalDevice& device)
	{
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

		// Go through each queue family and check if it has at least 1 of the required types of queue
		int i = 0;
		for (const auto& queueFamily : queueFamilyList)
		{
			if (queueFamily.queueCount > 0)
			{
				// At least one queue and it has graphic queue family (a queue could be multiple types)
				if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && MainDevice.QueueFamilyIndices.graphicFamily == -1)
				{
					MainDevice.QueueFamilyIndices.graphicFamily = i;
				}
				VkBool32 presentationSuppot = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, Surface, &presentationSuppot);
				// Check if queue is presentation type (can be both graphics and presentations)
				if (presentationSuppot && MainDevice.QueueFamilyIndices.presentationFamily == -1)
				{
					MainDevice.QueueFamilyIndices.presentationFamily = i;
				}

				// try to find a queue family index that support compute bit but not graphic and compute
				if ((queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && MainDevice.QueueFamilyIndices.computeFamily == -1)
				{
					MainDevice.QueueFamilyIndices.computeFamily = i;
				}
			}
			++i;
		}

	}

	void VKRenderer::getSwapChainDetail(const VkPhysicalDevice& device)
	{
		
		// Capabilities for the surface of this device
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, Surface, &SwapChainDetail.SurfaceCapabilities);

		// get list of support formats
		{
			uint32_t FormatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &FormatCount, nullptr);
			if (FormatCount > 0)
			{
				SwapChainDetail.ImgFormats.resize(FormatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &FormatCount, SwapChainDetail.ImgFormats.data());
			}
		}

		// get list of support presentation modes
		{
			uint32_t ModeCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &ModeCount, nullptr);
			if (ModeCount > 0)
			{
				SwapChainDetail.PresentationModes.resize(ModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &ModeCount, SwapChainDetail.PresentationModes.data());
			}
		}
	}



}