#include "VKRenderer.h"

#include "ComputePass.h"

#include "Camera.h"
#include "Mesh/Mesh.h"
#include "Texture/Texture.h"
#include "Transform/Transform.h"
#include "Model/Model.h"

#include <stdexcept>
#include "stdlib.h"
#include <set>
#include "assert.h"

#include "glm/gtc/matrix_transform.hpp"

#include "assimp/Importer.hpp"
#include <assimp/scene.h>
#include "assimp/postprocess.h"
namespace VKE
{
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
			// Descriptor set and push constant related
			{
				// fill the Descriptor list, this function should be call first
				createUniformBuffer();
				createDescriptorPool();
				createDescriptorSetLayout();
				allocateDescriptorSetsAndUpdateDescriptorSetWrites();
				createPushConstantRange();
			}

			createGraphicsPipeline();

			createFrameBuffer();
			createCommandPool();
			createCommandBuffers();
			createSynchronization();

			// Create Texture
			cTexture::Load("DefaultWhite.png", MainDevice);
			cTexture::Load("brick.png", MainDevice);
			cTexture::Load("panda.jpg", MainDevice);
			cTexture::Load("teapot.png", MainDevice);

			// Create compute pass
			//pCompute = DBG_NEW FComputePass();
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
		RenderList[0]->Transform.gRotate(cTransform::WorldUp, dt);
		RenderList[0]->Transform.Update();
	}


	void VKRenderer::prepareForDraw()
	{
		int CurrentFrame = ElapsedFrame % MAX_FRAME_DRAWS;
		// Wait for given fence to be opened from last draw before continuing
		vkWaitForFences(MainDevice.LD, 1, &DrawFences[CurrentFrame],
			VK_TRUE,													// Must wait for all fences to be opened(signaled) to pass this wait
			std::numeric_limits<uint64_t>::max());						// No time-out
		vkResetFences(MainDevice.LD, 1, &DrawFences[CurrentFrame]);		// Need to close(reset) this fence manually

		/** I. get the next available image to draw to and signal(semaphore1) when we're finished with the image */
		SwapChain.acquireNextImage(MainDevice, OnImageAvailables[CurrentFrame]);
	}

	void VKRenderer::presentFrame()
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
		RESULT_CHECK(Result, "Fail to Present Image");
	}

	void VKRenderer::draw()
	{
		int CurrentFrame = ElapsedFrame % MAX_FRAME_DRAWS;
		prepareForDraw();
		// Record commands
		recordCommands();
		// Update uniform buffer
		updateUniformBuffers();

		/** II. submit command buffer to queue (graphic queue) for execution, make sure it waits for the image to be signaled as available before drawing,
		 and signals (semaphore2) when it has finished rendering.*/
		 // Queue Submission information
		VkSubmitInfo SubmitInfo = {};
		SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		SubmitInfo.waitSemaphoreCount = 1;								// Number of semaphores to wait on
		SubmitInfo.pWaitSemaphores = &OnImageAvailables[CurrentFrame];	// It can start the command buffers all the way before it can draw to the screen.
		VkPipelineStageFlags WaitStages[] =
		{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT				// Seen this in subpass dependency, command buffers will run until this stage
		};
		SubmitInfo.pWaitDstStageMask = WaitStages;						// Stages to check semaphores at
		SubmitInfo.commandBufferCount = 1;								// Number of command buffers to submit, only submit to one frame once
		SubmitInfo.pCommandBuffers = &CommandBuffers[SwapChain.ImageIndex];		// Command buffer to submit
		SubmitInfo.signalSemaphoreCount = 1;							// Number of Semaphores to signal before the command buffer has finished
		SubmitInfo.pSignalSemaphores = &OnRenderFinisheds[CurrentFrame];// When the command buffer is finished, these semaphores will be signaled

		// This is the execute function also because the queue will execute commands automatically
		// When finish those commands, open(signal) this fence
		VkResult Result = vkQueueSubmit(MainDevice.graphicQueue, 1, &SubmitInfo, DrawFences[CurrentFrame]);
		RESULT_CHECK(Result, "Fail to submit command buffers to graphic queue");

		/** III. present image to screen when it has signaled finished rendering */
		presentFrame();

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

		vkDestroyDescriptorPool(MainDevice.LD, InputDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(MainDevice.LD, InputSetLayout, nullptr);

		vkDestroyDescriptorPool(MainDevice.LD, SamplerDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(MainDevice.LD, SamplerSetLayout, nullptr);

		cTexture::Free();

		// Clean up render list
		for (auto Model : RenderList)
		{
			Model->cleanUp();
			safe_delete(Model);
		}
		RenderList.clear();
		// Clear all mesh assets
		cMesh::Free();

		for (size_t i = 0; i < MAX_FRAME_DRAWS; ++i)
		{
			vkDestroySemaphore(MainDevice.LD, OnRenderFinisheds[i], nullptr);
			vkDestroySemaphore(MainDevice.LD, OnImageAvailables[i], nullptr);
			vkDestroyFence(MainDevice.LD, DrawFences[i], nullptr);
		}
		vkDestroyCommandPool(MainDevice.LD, MainDevice.GraphicsCommandPool, nullptr);
		for (auto& FrameBuffer : SwapChainFramebuffers)
		{
			vkDestroyFramebuffer(MainDevice.LD, FrameBuffer, nullptr);
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

			vkDestroyDescriptorSetLayout(MainDevice.LD, DescriptorSetLayout, nullptr);

			for (size_t i = 0; i < SwapChain.Images.size(); ++i)
			{
				Descriptor_Frame[i].cleanUp();
				Descriptor_Drawcall[i].cleanUp();

			}
		}

		{
			// Destroy pipelines first and then destroy render pass
			vkDestroyPipeline(MainDevice.LD, SecondGraphicPipeline, nullptr);
			vkDestroyPipelineLayout(MainDevice.LD, SecondPipelineLayout, nullptr);

			vkDestroyPipeline(MainDevice.LD, GraphicPipeline, nullptr);
			vkDestroyPipelineLayout(MainDevice.LD, PipelineLayout, nullptr);

			vkDestroyRenderPass(MainDevice.LD, RenderPass, nullptr);
		}

		for (auto & Image : SwapChain.Images)
		{
			vkDestroyImageView(MainDevice.LD, Image.ImgView, nullptr);
		}
		vkDestroySwapchainKHR(MainDevice.LD, SwapChain.SwapChain, nullptr);
		vkDestroySurfaceKHR(vkInstance, Surface, nullptr);
		vkDestroyDevice(MainDevice.LD, nullptr);
		vkDestroyInstance(vkInstance, nullptr);
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
		std::set<int> queueFamilyIndices = { MainDevice.QueueFamilyIndices.graphicFamily, MainDevice.QueueFamilyIndices.presentationFamily };
		for (auto& queueFamilyIdx : queueFamilyIndices)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamilyIdx;											// Index of the family to create a queue from
			queueCreateInfo.queueCount = 1;
			float priority = 1.0f;
			queueCreateInfo.pQueuePriorities = &priority;												// If there are multiple queues, GPU needs to know the execution order of different queues (1.0 == highest prioity)
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
		// Pick best settings that are supported
		FSwapChainDetail SwapChainDetail = getSwapChainDetail(MainDevice.PD);

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

		SwapChain.Images.reserve(SwapChainImageCount);

		for (auto & Image : Images)
		{
			FSwapChainImage SwapChainImage = {};
			SwapChainImage.Image = Image;
			SwapChainImage.ImgView = CreateImageViewFromImage(&MainDevice, Image, SwapChain.ImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

			SwapChain.Images.push_back(SwapChainImage);
		}
		assert(MAX_FRAME_DRAWS < Images.size());
		printf("%d Image view has been created\n", SwapChainImageCount);
	}

	void VKRenderer::createRenderPass()
	{
		// Array of sub-passes
		const uint32_t SubpassCount = 2;
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

		// Setup sub-pass 1
		Subpasses[1] = Helpers::SubpassDescriptionDefault(VK_PIPELINE_BIND_POINT_GRAPHICS);
		Subpasses[1].colorAttachmentCount = 1;
		Subpasses[1].pColorAttachments = &SwapChainColorAttachmentReference;				// Color attachment references
		Subpasses[1].inputAttachmentCount = InputReferenceCount;
		Subpasses[1].pInputAttachments = InputReferences;

		/** 3. Need to determine when layout transitions occur using sub-pass dependencies */
		const uint32_t DependencyCount = 3;
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

		// 3.2 Sub-pass 0 (output color / depth) to sub-pass 1 (shader read)
		SubpassDependencies[1].srcSubpass = 0;
		SubpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// Color attachment finish output before trying to read it.
		SubpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;			// Since it is output, which means it is write bit

		SubpassDependencies[1].dstSubpass = 1;
		SubpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;			// Access shader-read-only part of the two attachments on the fragment shader stage, so need to finish before this stage
		SubpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;						// Before the shader tries to read it
		SubpassDependencies[1].dependencyFlags = 0;

		// 3.3 sub-pass 1 to External
		// Conversion from VK_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		SubpassDependencies[2].srcSubpass = 1;
		SubpassDependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		SubpassDependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		SubpassDependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
		SubpassDependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		SubpassDependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		SubpassDependencies[2].dependencyFlags = 0;

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

	void VKRenderer::createDescriptorSetLayout()
	{
		// UNIFORM DESCRIPTOR SET LAYOUT
		{
			const uint32_t BindingCount = 2;
			VkDescriptorSetLayoutBinding Bindings[BindingCount] = {};
			// FrameData binding info
			Bindings[0] = Descriptor_Frame[0].ConstructDescriptorSetLayoutBinding();

			// Drawcall data binding info
			Bindings[1] = Descriptor_Drawcall[0].ConstructDescriptorSetLayoutBinding();

			VkDescriptorSetLayoutCreateInfo LayoutCreateInfo;
			LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			LayoutCreateInfo.bindingCount = BindingCount;
			LayoutCreateInfo.pBindings = Bindings;
			LayoutCreateInfo.pNext = nullptr;
			LayoutCreateInfo.flags = 0;

			VkResult Result = vkCreateDescriptorSetLayout(MainDevice.LD, &LayoutCreateInfo, nullptr, &DescriptorSetLayout);
			RESULT_CHECK(Result, "Fail to create descriptor set layout.")
		}

		// SAMPLER DESCRIPTOR LAYOUT
		{
			const uint32_t BindingCount = 2;
			VkDescriptorSetLayoutBinding Bindings[BindingCount] = {};

			// Albedo map
			Bindings[0].binding = 0;
			Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			Bindings[0].descriptorCount = 1;
			Bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			Bindings[0].pImmutableSamplers = nullptr;

			// Normal map
			Bindings[1].binding = 1;
			Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			Bindings[1].descriptorCount = 1;
			Bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			Bindings[1].pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutCreateInfo TextureLayoutCreateInfo = {};
			TextureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			TextureLayoutCreateInfo.bindingCount = BindingCount;
			TextureLayoutCreateInfo.pBindings = Bindings;
			TextureLayoutCreateInfo.pNext = nullptr;
			TextureLayoutCreateInfo.flags = 0;

			VkResult Result = vkCreateDescriptorSetLayout(MainDevice.LD, &TextureLayoutCreateInfo, nullptr, &SamplerSetLayout);
			RESULT_CHECK(Result, "Fail to create sampler descriptor set layout.")
		}

		// INPUT DESCRIPTOR LAYOUT
		{
			const uint32_t BindingCount = 2;
			VkDescriptorSetLayoutBinding Bindings[BindingCount] = {};
			Bindings[0].binding = 0;
			Bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			Bindings[0].descriptorCount = 1;
			Bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			Bindings[0].pImmutableSamplers = nullptr;

			Bindings[1].binding = 1;
			Bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			Bindings[1].descriptorCount = 1;
			Bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			Bindings[1].pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutCreateInfo InputLayoutCreateInfo = {};
			InputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			InputLayoutCreateInfo.bindingCount = BindingCount;
			InputLayoutCreateInfo.pBindings = Bindings;
			InputLayoutCreateInfo.pNext = nullptr;
			InputLayoutCreateInfo.flags = 0;

			VkResult Result = vkCreateDescriptorSetLayout(MainDevice.LD, &InputLayoutCreateInfo, nullptr, &InputSetLayout);
			RESULT_CHECK(Result, "Fail to create Input descriptor set layout.")
		}
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
		VertexBindDescription.binding = 0;										// Can bind multiple streams of data, this defines which one
		VertexBindDescription.stride = sizeof(FVertex);							// Size of of a single vertex object
		VertexBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;			// Define how to move between data after each vertex, 
																				// VK_VERTEX_INPUT_RATE_VERTEX : move to the next vertex
																				// VK_VERTEX_INPUT_RATE_INSTANCE : Move to a vertex for the next instance

		// How the data for an attribute is defined within a vertex
		const uint32_t AttrubuteDescriptionCount = 3;
		VkVertexInputAttributeDescription AttributeDescriptions[AttrubuteDescriptionCount];

		// Position attribute
		AttributeDescriptions[0].binding = 0;									// This binding corresponds to the layout(binding = 0, location = 0) in vertex shader, should be same as above
		AttributeDescriptions[0].location = 0;									// This binding corresponds to the layout(binding = 0, location = 0) in vertex shader, this is a position data
		AttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;			// format of the data, defnie the size of the data, 3 * 32 bit float data
		AttributeDescriptions[0].offset = offsetof(FVertex, Position);			// Similar stride concept, position start at 0, but the following attribute data should has offset of sizeof(glm::vec3)
		// Color attribute ...
		AttributeDescriptions[1].binding = 0;
		AttributeDescriptions[1].location = 1;
		AttributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		AttributeDescriptions[1].offset = offsetof(FVertex, Color);
		// texture coordinate attribute
		AttributeDescriptions[2].binding = 0;
		AttributeDescriptions[2].location = 2;
		AttributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		AttributeDescriptions[2].offset = offsetof(FVertex, TexCoord);

		VertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		VertexInputCreateInfo.pVertexBindingDescriptions = &VertexBindDescription;				// list of vertex binding description data spacing, stride info
		VertexInputCreateInfo.vertexAttributeDescriptionCount = AttrubuteDescriptionCount;
		VertexInputCreateInfo.pVertexAttributeDescriptions = AttributeDescriptions;				// data format where to bind in shader


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

		ColorStateAttachments.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		ColorStateAttachments.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
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
		VkDescriptorSetLayout Layouts[SetLayoutCount] = { DescriptorSetLayout, SamplerSetLayout };

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

		/** 2. Create second Pipeline*/
		{
			// === Read in SPIR-V code of shaders === 
			auto VertexShaderCode1 = FileIO::ReadFile("Content/Shaders/bigTriangle.spv");
			auto FragShaderCode1 = FileIO::ReadFile("Content/Shaders/second.spv");

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

			// No vertex data for the second pass
			VertexInputCreateInfo.vertexBindingDescriptionCount = 0;
			VertexInputCreateInfo.pVertexBindingDescriptions = nullptr;
			VertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
			VertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;

			// disable depth / stencil testing
			DepthStencilCreateInfo.depthWriteEnable = VK_FALSE;

			// Create Another pipeline layout
			VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo1 = {};
			PipelineLayoutCreateInfo1.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			PipelineLayoutCreateInfo1.setLayoutCount = 1;
			PipelineLayoutCreateInfo1.pSetLayouts = &InputSetLayout;
			PipelineLayoutCreateInfo1.pushConstantRangeCount = 0;
			PipelineLayoutCreateInfo1.pPushConstantRanges = nullptr;

			Result = vkCreatePipelineLayout(MainDevice.LD, &PipelineLayoutCreateInfo1, nullptr, &SecondPipelineLayout);
			RESULT_CHECK(Result, "Fail to create the second pipeline layout");

			// Update PipelineCraeteInfo according to the previous changes
			PipelineCreateInfo.pStages = ShaderStages1;
			PipelineCreateInfo.layout = SecondPipelineLayout;
			PipelineCreateInfo.subpass = 1;	// Which sub-pass this pipeline is in 

			// Create the second pipeline 
			Result = vkCreateGraphicsPipelines(MainDevice.LD, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &SecondGraphicPipeline);
			RESULT_CHECK(Result, "Fail to create the second Graphics Pipelines.");
		}

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

		VkCommandBufferAllocateInfo cbAllocInfo = {};						// Memory exists already, only get it from the pool
		cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbAllocInfo.commandPool = MainDevice.GraphicsCommandPool;						// Only works for graphics command pool
		cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;				// VK_COMMAND_BUFFER_LEVEL_PRIMARY: Submitted directly to queue, can not be called by another command buffer
																			// VK_COMMAND_BUFFER_LEVEL_SECONDARY: Command buffer within a command buffer e.g. VkCmdExecuteCommands(another command buffer)
		cbAllocInfo.commandBufferCount = static_cast<uint32_t>(CommandBuffers.size());		// Allows allocating multiple command buffers at the same time

		VkResult Result = vkAllocateCommandBuffers(MainDevice.LD, &cbAllocInfo, CommandBuffers.data());		// Don't need a custom allocation function
		RESULT_CHECK(Result, "Fail to allocate command buffers.");

		// Record the command buffer and call this buffer once and once again.
	}

	void VKRenderer::createSynchronization()
	{
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
			VkResult Result = vkCreateSemaphore(MainDevice.LD, &SemaphoreCreateInfo, nullptr, &OnImageAvailables[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create OnImageAvailables[%d] Semaphore", i);

			Result = vkCreateSemaphore(MainDevice.LD, &SemaphoreCreateInfo, nullptr, &OnRenderFinisheds[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create OnRenderFinisheds[%d] Semaphore", i);

			Result = vkCreateFence(MainDevice.LD, &FenceCreateInfo, nullptr, &DrawFences[i]);
			RESULT_CHECK_ARGS(Result, "Fail to create DrawFences[%d] Fence", i);
		}
	}



	void VKRenderer::createUniformBuffer()
	{
		// One uniform buffer for each image (and by extension, command buffer)
		size_t Size = SwapChain.Images.size();
		Descriptor_Frame.resize(Size);
		Descriptor_Drawcall.resize(Size);

		// Create UniformBuffers
		for (size_t i = 0; i < Size; ++i)
		{
			// Only needs 1 FFrame per pass
			Descriptor_Frame[i].SetDescriptorBufferRange(sizeof(BufferFormats::FFrame), 1);
			Descriptor_Frame[i].CreateDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, VK_SHADER_STAGE_VERTEX_BIT,
				MainDevice.PD, MainDevice.LD);

			// Needs MAX_OBJECTS FDrawcall per pass
			Descriptor_Drawcall[i].SetDescriptorBufferRange(sizeof(BufferFormats::FDrawCall), MAX_OBJECTS);
			Descriptor_Drawcall[i].CreateDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT,
				MainDevice.PD, MainDevice.LD);
		}
	}

	void VKRenderer::createDescriptorPool()
	{
		// Type of descriptors + how many DESCRIPTORS, not DESCRIPTOR Sets (combined makes the pool size)
		const uint32_t DescriptorTypeCount = 2;
		VkDescriptorPoolSize PoolSize[DescriptorTypeCount] = {};
		PoolSize[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		PoolSize[0].descriptorCount = static_cast<uint32_t>(SwapChain.Images.size());
		PoolSize[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		PoolSize[1].descriptorCount = static_cast<uint32_t>(SwapChain.Images.size());

		VkDescriptorPoolCreateInfo PoolCreateInfo = {};
		PoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		PoolCreateInfo.maxSets = static_cast<uint32_t>(SwapChain.Images.size());
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

		// CREATE INPUT DESCRIPTOR POOL
		{
			// Color attachment pool size
			VkDescriptorPoolSize ColorInputPoolSize = {};
			ColorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			ColorInputPoolSize.descriptorCount = static_cast<uint32_t>(ColorBuffers.size());

			// Depth attachment pool size
			VkDescriptorPoolSize DepthInputPoolSize = {};
			DepthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
			DepthInputPoolSize.descriptorCount = static_cast<uint32_t>(DepthBuffers.size());

			const uint32_t PoolSizeCount = 2;
			VkDescriptorPoolSize PoolSizes[PoolSizeCount] = { ColorInputPoolSize , DepthInputPoolSize };

			VkDescriptorPoolCreateInfo InputPoolCreateInfo = {};
			InputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			InputPoolCreateInfo.maxSets = ColorBuffers.size(); // Could be DepthBuffers.size() too, it is the same
			InputPoolCreateInfo.poolSizeCount = PoolSizeCount;
			InputPoolCreateInfo.pPoolSizes = PoolSizes;

			Result = vkCreateDescriptorPool(MainDevice.LD, &InputPoolCreateInfo, nullptr, &InputDescriptorPool);
			RESULT_CHECK(Result, "Failed to create a Sampler Descriptor Pool");

		}
	}

	void VKRenderer::allocateDescriptorSetsAndUpdateDescriptorSetWrites()
	{
		// DESCRIPTOR SETS
		{
			DescriptorSets.resize(SwapChain.Images.size());
			// Fill array of layouts ready for set creation
			std::vector<VkDescriptorSetLayout> SetLayouts(SwapChain.Images.size(), DescriptorSetLayout);

			/** 1. Allocate descriptor set from the pool together */
			VkDescriptorSetAllocateInfo SetAllocInfo = {};
			SetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			SetAllocInfo.descriptorPool = DescriptorPool;													// Pool to allocate descriptor set from
			SetAllocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain.Images.size());				// Number of sets to allocate
			SetAllocInfo.pSetLayouts = SetLayouts.data();

			// Allocate descriptor sets (multiple)
			VkResult Result = vkAllocateDescriptorSets(MainDevice.LD, &SetAllocInfo, DescriptorSets.data());
			RESULT_CHECK(Result, "Fail to Allocate Descriptor Set!");

			/** 2. Update all of descriptor set buffer bindings */
			for (size_t i = 0; i < SwapChain.Images.size(); ++i)
			{
				Descriptor_Frame[i].SetDescriptorSet(&DescriptorSets[i]);
				Descriptor_Drawcall[i].SetDescriptorSet(&DescriptorSets[i]);

				const uint32_t SetWriteCount = 2;
				// Data about connection between Descriptor and buffer
				VkWriteDescriptorSet SetWrites[SetWriteCount] = {};

				// FRAME DESCRIPTOR
				SetWrites[0] = Descriptor_Frame[i].ConstructDescriptorBindingInfo();
				// DRAWCALL DESCRIPTOR
				SetWrites[1] = Descriptor_Drawcall[i].ConstructDescriptorBindingInfo();

				// Update the descriptor sets with new buffer / binding info
				vkUpdateDescriptorSets(MainDevice.LD, SetWriteCount, SetWrites,
					0, nullptr // Allows copy descriptor set to another descriptor set
				);
			}
		}
		// INPUT DESCRIPTOR SETS
		{
			InputDescriptorSets.resize(SwapChain.Images.size());
			// Fill array of layouts ready for set creation
			std::vector<VkDescriptorSetLayout> InputSetLayouts(SwapChain.Images.size(), InputSetLayout);
			VkDescriptorSetAllocateInfo InputSetAllocInfo = {};
			InputSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			InputSetAllocInfo.descriptorPool = InputDescriptorPool;
			InputSetAllocInfo.descriptorSetCount = static_cast<uint32_t>(SwapChain.Images.size());
			InputSetAllocInfo.pSetLayouts = InputSetLayouts.data();

			// Allocate input descriptor sets (multiple)
			VkResult Result = vkAllocateDescriptorSets(MainDevice.LD, &InputSetAllocInfo, InputDescriptorSets.data());
			RESULT_CHECK(Result, "Fail to Allocate Input Descriptor Set!");

			for (size_t i = 0; i < SwapChain.Images.size(); ++i)
			{
				// Color Attachment Descriptor
				VkDescriptorImageInfo ColorAttachmentDescriptor = {};
				ColorAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				ColorAttachmentDescriptor.imageView = ColorBuffers[i].GetImageView();
				ColorAttachmentDescriptor.sampler = VK_NULL_HANDLE;		// Can not use sampler since it is a input attachment

				VkWriteDescriptorSet ColorWrite = {};
				ColorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				ColorWrite.dstSet = InputDescriptorSets[i];
				ColorWrite.dstBinding = 0;
				ColorWrite.dstArrayElement = 0;
				ColorWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				ColorWrite.descriptorCount = 1;
				ColorWrite.pImageInfo = &ColorAttachmentDescriptor;

				// Depth Attachment Descriptor
				VkDescriptorImageInfo DepthAttachmentDescriptor = {};
				DepthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				DepthAttachmentDescriptor.imageView = DepthBuffers[i].GetImageView();
				DepthAttachmentDescriptor.sampler = VK_NULL_HANDLE;		// Can not use sampler since it is a input attachment

				VkWriteDescriptorSet DepthWrite = {};
				DepthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				DepthWrite.dstSet = InputDescriptorSets[i];
				DepthWrite.dstBinding = 1;
				DepthWrite.dstArrayElement = 0;
				DepthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				DepthWrite.descriptorCount = 1;
				DepthWrite.pImageInfo = &DepthAttachmentDescriptor;

				const uint32_t SetWriteCount = 2;
				// Data about connection between Descriptor and buffer
				VkWriteDescriptorSet SetWrites[SetWriteCount] = { ColorWrite, DepthWrite };

				// Update the descriptor sets with new buffer / binding info
				vkUpdateDescriptorSets(MainDevice.LD, SetWriteCount, SetWrites,
					0, nullptr // Allows copy descriptor set to another descriptor set
				);
			}
		}
	}

	void VKRenderer::updateUniformBuffers()
	{
		int idx = SwapChain.ImageIndex;
		// Copy Frame data
		Descriptor_Frame[idx].UpdateBufferData(&GetCurrentCamera()->GetFrameData());

		// Update model data to pDrawcallTransferSpace
		for (size_t i = 0; i < RenderList.size(); ++i)
		{
			using namespace BufferFormats;
			FDrawCall* Drawcall = reinterpret_cast<FDrawCall*>(reinterpret_cast<uint64_t>(Descriptor_Drawcall[idx].GetAllocatedMemory()) + (i * Descriptor_Drawcall[idx].GetSlotSize()));
			*Drawcall = RenderList[i]->Transform.M();
		}
		// Copy Model data
		// Reuse void* Data
		size_t DBufferSize = static_cast<size_t>(Descriptor_Drawcall[idx].GetSlotSize()) * RenderList.size();
		Descriptor_Drawcall[idx].UpdatePartialData(Descriptor_Drawcall[idx].GetAllocatedMemory(), 0, DBufferSize);
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
		if (!getSwapChainDetail(device).IsValid())
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

	bool VKRenderer::CreateModel(const std::string& ifileName, cModel*& oModel)
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
				Mesh->CreateDescriptorSet(SamplerSetLayout, SamplerDescriptorPool);
			}
		}
		oModel = DBG_NEW cModel(Meshes);

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
		ClearValues[1].color = { 0.4f, 0.4f, 0.4f, 1.0f };							// Color attachment clear value
		ClearValues[2].depthStencil.depth = 1.0f;									// Depth attachment clear value

		RenderPassBeginInfo.pClearValues = ClearValues;								// List of clear values 
		RenderPassBeginInfo.clearValueCount = ClearColorCount;

		RenderPassBeginInfo.framebuffer = SwapChainFramebuffers[SwapChain.ImageIndex];

		// Start recording commands to command buffer
		VkResult Result = vkBeginCommandBuffer(CB, &BufferBeginInfo);
		RESULT_CHECK_ARGS(Result, "Fail to start recording a command buffer[%d]", SwapChain.ImageIndex);

		/** Record part */
		// Begin Render Pass
		vkCmdBeginRenderPass(CB, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Start the first sub-pass
		// Bind Pipeline to be used in render pass
		vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicPipeline);
		/*
			Deferred shading example:
			G-Buffer pipeline, stores all data to multiple attachments
			Switch to another pipeline, do lighting
			Switch to another pipeline, do HDR
			...
		*/

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
				vkCmdBindVertexBuffers(CB, 0, 1, VertexBuffers, Offsets);	// Command to bind vertex buffer for drawing with

				// Only one index buffer is allowed, it handles all vertex buffer's index, uint32 type is more than enough for the index count
				vkCmdBindIndexBuffer(CB, Mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

				// Dynamic Offset Amount
				uint32_t DynamicOffset = static_cast<uint32_t>(Descriptor_Drawcall[SwapChain.ImageIndex].GetSlotSize()) * j;

				const uint32_t DescriptorSetCount = 2;
				// Two descriptor sets
				VkDescriptorSet DescriptorSetGroup[] = { *Descriptor_Drawcall[SwapChain.ImageIndex].GetDescriptorSet(), Mesh->GetDescriptorSet() };

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
			vkCmdBindPipeline(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, SecondGraphicPipeline);
			// No need to bind vertex buffer or index buffer
			vkCmdBindDescriptorSets(CB, VK_PIPELINE_BIND_POINT_GRAPHICS, SecondPipelineLayout,
				0, 1, &InputDescriptorSets[SwapChain.ImageIndex],
				0, nullptr);	// no dynamic offset
			// Draw 3 vertex (1 triangle) only 
			vkCmdDraw(CB, 3, 1, 0, 0);
		}
		// End Render Pass
		vkCmdEndRenderPass(CB);

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

	FSwapChainDetail VKRenderer::getSwapChainDetail(const VkPhysicalDevice& device)
	{
		FSwapChainDetail SwapChainDetails;

		// Capabilities for the surface of this device
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, Surface, &SwapChainDetails.SurfaceCapabilities);

		// get list of support formats
		{
			uint32_t FormatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &FormatCount, nullptr);
			if (FormatCount > 0)
			{
				SwapChainDetails.ImgFormats.resize(FormatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &FormatCount, SwapChainDetails.ImgFormats.data());
			}
		}

		// get list of support presentation modes
		{
			uint32_t ModeCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &ModeCount, nullptr);
			if (ModeCount > 0)
			{
				SwapChainDetails.PresentationModes.resize(ModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &ModeCount, SwapChainDetails.PresentationModes.data());
			}
		}

		return SwapChainDetails;
	}

}