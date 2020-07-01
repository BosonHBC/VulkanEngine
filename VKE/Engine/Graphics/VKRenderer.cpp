#include "VKRenderer.h"
#include <stdexcept>
#include "stdlib.h"
#include <set>

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
			createRenderPass();
			createGraphicsPipeline();

		}
		catch (const std::runtime_error &e)
		{
			printf("ERROR: %s \n", e.what());
			return EXIT_FAILURE;
		}

		return EXIT_SUCCESS;
	}

	void VKRenderer::cleanUp()
	{
		vkDestroyPipeline(MainDevice.LD, GraphicPipeline, nullptr);
		vkDestroyPipelineLayout(MainDevice.LD, PipelineLayout, nullptr);
		vkDestroyRenderPass(MainDevice.LD, RenderPass, nullptr);
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
		auto result = vkCreateInstance(&CreateInfo, nullptr, &vkInstance);
		if (result != VkResult::VK_SUCCESS)
		{
			char ErrorMsg[100];
			sprintf_s(ErrorMsg, "Fail to Create VKInstance, exit code: %d\n", result);
			throw std::runtime_error(ErrorMsg);
		}
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
	}

	void VKRenderer::createLogicalDevice()
	{
		// Get the queue family indices for the chosen PD
		FQueueFamilyIndices indieces = getQueueFamilies(MainDevice.PD);

		// Queue is needed to create a logical device,
		// vector for queue create information
		std::vector< VkDeviceQueueCreateInfo> queueCreateInfos;
		// set of queue family indices, prevent duplication
		std::set<int> queueFamilyIndices = { indieces.graphicFamily, indieces.presentationFamily };
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

		DeviceCreateInfo.pEnabledFeatures = &PDFeatures;

		VkResult result = vkCreateDevice(MainDevice.PD, &DeviceCreateInfo, nullptr, &MainDevice.LD);
		if (result != VkResult::VK_SUCCESS)
		{
			char ErrorMsg[100];
			sprintf_s(ErrorMsg, "Fail to Create VKLogical Device, exit code: %d\n", result);
			throw std::runtime_error(ErrorMsg);
		}

		// After LD is created, queues should be created too, Save the queues
		vkGetDeviceQueue(
			/*From given LD*/MainDevice.LD,
			/*given queue family*/ indieces.graphicFamily,
			/*given queue index(0 only one queue)*/ 0,
			/*Out queue*/ &graphicQueue);
		vkGetDeviceQueue(MainDevice.LD, indieces.presentationFamily, 0, &presentationQueue);
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
		if (glfwCreateWindowSurface(vkInstance, window, nullptr, &Surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Fail to create a surface");
		}
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

		// Get queue family indices
		FQueueFamilyIndices Indices = getQueueFamilies(MainDevice.PD);

		// Images are sharing between two queues
		if (Indices.graphicFamily != Indices.presentationFamily)
		{
			uint32_t QueueFamilyIndices[] =
			{
				static_cast<uint32_t>(Indices.graphicFamily),
				static_cast<uint32_t>(Indices.presentationFamily),
			};
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
		if (Result != VK_SUCCESS)
		{
			throw std::runtime_error("Fail to create SwapChain");
		}

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
			SwapChainImage.ImgView = CreateImageViewFromImage(Image, SwapChain.ImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

			SwapChain.Images.push_back(SwapChainImage);
		}
		printf("%d Image view has been created\n", SwapChainImageCount);
	}

	void VKRenderer::createRenderPass()
	{
		// 1. Create Attachments of render pass
		VkAttachmentDescription ColorAttachment0 = {};

		ColorAttachment0.format = SwapChain.ImageFormat;						// Format to use for this attachment
		ColorAttachment0.samples = VK_SAMPLE_COUNT_1_BIT;						// Number of samples to write for Sampling
		ColorAttachment0.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;					// Define what happen before rendering
		ColorAttachment0.storeOp = VK_ATTACHMENT_STORE_OP_STORE;				// Define what happen after rendering
		ColorAttachment0.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;		// Don't care about stencil before rendering
		ColorAttachment0.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		// Don't care about stencil after rendering

		// Frame buffer data will be stored as an image, but images can be given different data layouts
		// to give optimal use for certain operations

		// Assume there are multiple sub-passes and all need to process the colorAttachment, its data layout may change between different sub-passes.
		// After all sub-passes has done with their work, the attachment should change to the finalLayout.
		// Initial layout -> subpasses1 layout -> ... -> subpassN layout -> final layout
		ColorAttachment0.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;				// Image data layout before render pass starts
		ColorAttachment0.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;			// Image data layout after render pass (to change to) for final purpose


		// Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
		VkAttachmentReference ColorAttachmentReference = {};

		ColorAttachmentReference.attachment = 0;										// It can refer to the same attachment, but with different layout
		ColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;		// optimal layout for color attachment


		// 2. Create sub-passes
		VkSubpassDescription Subpass = {};

		Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;			// Pipeline type, (Graphics pipeline, Compute pipeline, RayTracing_NV...)
		Subpass.colorAttachmentCount = 1;
		Subpass.pColorAttachments = &ColorAttachmentReference;					// Color attachment references


		// Need to determine when layout transitions occur using subpass dependencies
		const uint32_t DependencyCount = 2;
		VkSubpassDependency SubpassDependencies[DependencyCount];

		// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		SubpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;								// external subpass, which is the c++ code
		SubpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;				// Stages of the pipeline, the transition happens right after the last of the pipeline
		SubpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;						// Happens after memory read 

		SubpassDependencies[0].dstSubpass = 0;													// fist subpass
		SubpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;	// transition happens before color attachment output stage
		SubpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		SubpassDependencies[0].dependencyFlags = 0;


		// Conversion from VK_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		SubpassDependencies[1].srcSubpass = 0;
		SubpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		SubpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		SubpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		SubpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		SubpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		SubpassDependencies[1].dependencyFlags = 0;



		// 3. Create Render Pass
		VkRenderPassCreateInfo RendePassCreateInfo = {};

		RendePassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		RendePassCreateInfo.attachmentCount = 1;
		RendePassCreateInfo.pAttachments = &ColorAttachment0;
		RendePassCreateInfo.subpassCount = 1;
		RendePassCreateInfo.pSubpasses = &Subpass;
		RendePassCreateInfo.dependencyCount = DependencyCount;
		RendePassCreateInfo.pDependencies = SubpassDependencies;


		VkResult Result = vkCreateRenderPass(MainDevice.LD, &RendePassCreateInfo, nullptr, &RenderPass);
		if (Result != VK_SUCCESS)
		{
			throw std::runtime_error("Fail to create render pass.");
		}

	}

	void VKRenderer::createGraphicsPipeline()
	{
		// Read in SPIR-V code of shaders
		auto VertexShaderCode = ReadFile("Content/Shaders/vert.spv");
		auto FragShaderCode = ReadFile("Content/Shaders/frag.spv");

		// Build Shader Module to link to Graphics Pipeline
		FShaderModuleScopeGuard VertexShaderModule, FragmentShaderModule;
		VertexShaderModule.CreateShaderModule(MainDevice.LD, VertexShaderCode);
		FragmentShaderModule.CreateShaderModule(MainDevice.LD, FragShaderCode);

		/* 1. Shader Creation Information stage*/
		// Vertex shader
		VkPipelineShaderStageCreateInfo VSCreateInfo = {};
		VSCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		VSCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;												// Shader type
		VSCreateInfo.module = VertexShaderModule.ShaderModule;											// Module used
		VSCreateInfo.pName = "main";																	// Entry Point name

		// Fragment shader
		VkPipelineShaderStageCreateInfo FSCreateInfo = {};
		FSCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		FSCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		FSCreateInfo.module = FragmentShaderModule.ShaderModule;
		FSCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo ShaderStages[] =
		{
			VSCreateInfo, FSCreateInfo
		};

		/** 2. Create Pipeline */
		VkGraphicsPipelineCreateInfo PieplineCreateInfo = {};

		// === Vertex Input === (@TODO: Putin vertex descripton when resources created)
		VkPipelineVertexInputStateCreateInfo VertexInputCreateInfo = {};
		VertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		VertexInputCreateInfo.vertexBindingDescriptionCount = 0;
		VertexInputCreateInfo.pVertexBindingDescriptions = nullptr;				// list of vertex binding description data spacing, stride info
		VertexInputCreateInfo.vertexAttributeDescriptionCount = 0;
		VertexInputCreateInfo.pVertexAttributeDescriptions = nullptr;			// data format where to bind in shader

		// === Input Assembly ===
		VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo = {};
		InputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		InputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;			// GL_TRIANGLE, GL_LINE, GL_POINT...
		InputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;						// Allow overriding of "strip" topology to start new primitives

		// === Viewport & Scissor ===
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

		VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};
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
		RasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;			// left-handed rule
		RasterizerCreateInfo.depthBiasEnable = VK_FALSE;					// Define the depth bias to fragments (good for stopping "shadow arcane")


		// === Multi-sampling ===
		VkPipelineMultisampleStateCreateInfo MSCreateInfo;
		MSCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		MSCreateInfo.pNext = nullptr;
		MSCreateInfo.sampleShadingEnable = VK_FALSE;						// Enable Multi-sampling or not
		MSCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;			// Number of samples to use per fragment
		MSCreateInfo.pSampleMask = nullptr;
		MSCreateInfo.flags = 0;

		// === Blending ===
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


		// Blending decides how to blend a new color being written to a fragment, with the old value
		VkPipelineColorBlendStateCreateInfo ColorBlendStateCreateInfo = {};

		ColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		ColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;					// Alternative to calculation is to use logical operations
		//ColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
		ColorBlendStateCreateInfo.attachmentCount = 1;
		ColorBlendStateCreateInfo.pAttachments = &ColorStateAttachments;


		// === Pipeline layout (@TODO: Apply future descriptor set layouts) ===
		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};

		PipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutCreateInfo.setLayoutCount = 0;
		PipelineLayoutCreateInfo.pSetLayouts = nullptr;
		PipelineLayoutCreateInfo.pushConstantRangeCount = 0;
		PipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

		// Create Pipeline layout
		VkResult result = vkCreatePipelineLayout(MainDevice.LD, &PipelineLayoutCreateInfo, nullptr, &PipelineLayout);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Fail to create Pipeline Layout.");
		}


		// === Depth Stencil testing ===
		// @TODO: Set up depth stencil testing

		// === Graphic Pipeline Creation ===
		VkGraphicsPipelineCreateInfo PipelineCreateInfo = {};

		PipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		PipelineCreateInfo.stageCount = 2;													// Shader stage count
		PipelineCreateInfo.pStages = ShaderStages;											// Shader create info
		PipelineCreateInfo.pVertexInputState = &VertexInputCreateInfo;
		PipelineCreateInfo.pInputAssemblyState = &InputAssemblyCreateInfo;
		PipelineCreateInfo.pViewportState = &ViewportStateCreateInfo;
		PipelineCreateInfo.pDynamicState = &DynamicStateCraeteInfo;
		PipelineCreateInfo.pRasterizationState = &RasterizerCreateInfo;
		PipelineCreateInfo.pMultisampleState = &MSCreateInfo;
		PipelineCreateInfo.pColorBlendState = &ColorBlendStateCreateInfo;
		PipelineCreateInfo.pDepthStencilState = nullptr;
		PipelineCreateInfo.layout = PipelineLayout;
		PipelineCreateInfo.renderPass = RenderPass;											// RenderPass description the pipeline is compatible with
		PipelineCreateInfo.subpass = 0;														// Subpass of render pass to use with pipeline

		// Pipeline Derivatives: Can create multiple pipelines that derive from one for optimization
		PipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;	//PipelineCreateInfo.basePipelineHandle = OldPipeline;	
		PipelineCreateInfo.basePipelineIndex = -1;

		// PipelineCache can save the cache when the next time create a pipeline
	    VkResult Result = vkCreateGraphicsPipelines(MainDevice.LD, VK_NULL_HANDLE, 1, &PipelineCreateInfo, nullptr, &GraphicPipeline);
		if (Result != VK_SUCCESS)
		{
			throw std::runtime_error("Fail to create Graphics Pipelines.");
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
		// Information about the device itself (ID, name, type, vendor, etc)
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		// Information about what the device supports (geo shader, tess shader, wide lines, etc)
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		FQueueFamilyIndices indices = getQueueFamilies(device);
		if (!indices.IsValid())
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

	VkImageView VKRenderer::CreateImageViewFromImage(const VkImage& iImage, const VkFormat& iFormat, const VkImageAspectFlags& iAspectFlags)
	{
		VkImageViewCreateInfo ViewCreateInfo = {};
		ViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ViewCreateInfo.image = iImage;
		ViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;						// Type of image (e.g. 2D, 3D, cubemap)
		ViewCreateInfo.format = iFormat;										// Format of the image (e.g. R8G8B8)
		ViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;			// Allows remapping rgba components to other rgba values
		ViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// SubResources allows the view to view only a part of am image
		ViewCreateInfo.subresourceRange.aspectMask = iAspectFlags;				// Which aspect of image to view (e.g. VK_IMAGE_ASPECT_COLOR_BIT for view)
		ViewCreateInfo.subresourceRange.baseMipLevel = 0;						// Start mipmap level to view from
		ViewCreateInfo.subresourceRange.levelCount = 1;							// Number of mipmap levels to view
		ViewCreateInfo.subresourceRange.baseArrayLayer = 0;						// Start array level to view from
		ViewCreateInfo.subresourceRange.layerCount = 1;							// Number of array levels to view 

		// Create Image view and return it
		VkImageView ImageView;
		VkResult Result = vkCreateImageView(MainDevice.LD, &ViewCreateInfo, nullptr, &ImageView);
		if (Result != VK_SUCCESS)
		{
			throw std::runtime_error("Fail to create an Image View");
		}

		return ImageView;
	}

	FQueueFamilyIndices VKRenderer::getQueueFamilies(const VkPhysicalDevice& device)
	{
		FQueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

		// Go through each queue family and check if it has at least 1 of the required types of queue
		int i = 0;
		for (const auto& queueFamily : queueFamilyList)
		{
			// At least one queue and it has graphic queue family (a queue could be multiple types)
			if (queueFamily.queueCount > 0 && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				indices.graphicFamily = i;
			}
			VkBool32 presentationSuppot = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, Surface, &presentationSuppot);
			// Check if queue is presentation type (can be both graphics and presentations)
			if (queueFamily.queueCount > 0 && presentationSuppot)
			{
				indices.presentationFamily = i;
			}
			++i;
		}

		return indices;
	}

	VKE::FSwapChainDetail VKRenderer::getSwapChainDetail(const VkPhysicalDevice& device)
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