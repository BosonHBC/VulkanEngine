// Engines
#include "Engine.h"

#include "Graphics/VKRenderer.h"
#include "Input/UserInput.h"
#include "Camera.h"
#include "Time.h"
// glm
#include "glm/glm.hpp"
#include "glm/mat4x4.hpp"
// Systems
#include "stdio.h"
#include <string>
#include "Model/Model.h"
// Imgui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"

namespace VKE {
	const GLuint WIDTH = 1280, HEIGHT = 720;
	const std::string WINDOW_NAME = "Default";
	uint64_t ElapsedFrame = 0;
	bool bWindowIconified = false;

	//=================== Parameters =================== 
	GLFWwindow* g_Window;
	VKRenderer* g_Renderer;
	UserInput::FUserInput* g_Input;
	cCamera* g_Camera;

	// Imgui
	ImGui_ImplVulkanH_Window g_MainWindowData;

	//=================== Function declarations =================== 

	// GLFW
	void initGLFW();
	void cleanupGLFW();
	// Input
	void initInput();
	void window_focus_callback(GLFWwindow* window, int focused);
	void window_iconify_callback(GLFWwindow* window, int iconified);
	void cleanupInput();
	void quitApp();

	// Camera
	void initCamera();
	void cleanupCamera();

	// Imgui
	void initImgui();
	void cleanupImgui();
	void check_vk_result(VkResult err);

	int init()
	{
		int result = EXIT_SUCCESS;
		initGLFW();
		initInput();
		initCamera();
		
		g_Renderer = DBG_NEW VKRenderer();
		if (g_Renderer->init(g_Window) == EXIT_FAILURE)
		{
			return EXIT_FAILURE;
		}
		initImgui();

		return result;
	}

	void run()
	{
		if (!g_Window)
		{
			return;
		}

		double LastTime = glfwGetTime();
		
		bool show_demo_window = true;
		bool show_another_window = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		while (!glfwWindowShouldClose(g_Window))
		{
			glfwPollEvents();

			double now = glfwGetTime();
			Time::DT = now - LastTime;
			
			LastTime = now;
			// not minimized
			if (!bWindowIconified)
			{
				// imgui stuffs
				// Start the Dear ImGui frame
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();

				// Imgui windows
				{
					// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
					if (show_demo_window)
						ImGui::ShowDemoWindow(&show_demo_window);

					// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
					{
						static float f = 0.0f;
						static int counter = 0;

						ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

						ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
						ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
						ImGui::Checkbox("Another Window", &show_another_window);

						ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
						ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

						if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
							counter++;
						ImGui::SameLine();
						ImGui::Text("counter = %d", counter);

						ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
						ImGui::End();
					}

					// 3. Show another simple window.
					if (show_another_window)
					{
						ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
						ImGui::Text("Hello from another window!");
						if (ImGui::Button("Close Me"))
							show_another_window = false;
						ImGui::End();
					}
				}

				g_Renderer->tick((float)Time::DT);
				g_Renderer->draw();

				g_Input->UpdateInput();
				g_Camera->Update();


			}
		}
	}

	void cleanup()
	{
		cleanupImgui();

		g_Renderer->cleanUp();
		safe_delete(g_Renderer);

		cleanupCamera();
		cleanupInput();
		cleanupGLFW();
	}


	GLFWwindow* GetGLFWWindow()
	{
		return g_Window;
	}

	cCamera* GetCurrentCamera()
	{
		return g_Camera;
	}

	glm::ivec2 GetWindowExtent()
	{
		return glm::ivec2(WIDTH, HEIGHT);
	}

	glm::vec2 GetMouseDelta()
	{
		if (g_Input)
		{
			return g_Input->GetMouseDelta();
		}
		return glm::vec2(0.0f);
	}

	void initGLFW()
	{
		glfwInit();

		// using Vulkan here
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

		g_Window = glfwCreateWindow(WIDTH, HEIGHT, WINDOW_NAME.c_str(), nullptr, nullptr);

		glfwSetWindowFocusCallback(g_Window, window_focus_callback);
		glfwSetWindowIconifyCallback(g_Window, window_iconify_callback);
	}
	void cleanupGLFW()
	{
		glfwDestroyWindow(g_Window);

		glfwTerminate();
	}

	void initInput()
	{
		using namespace UserInput;
		g_Input = DBG_NEW UserInput::FUserInput();
		g_Input->AddActionKeyPairToMap("Escape", EKeyCode::Escape);
		g_Input->BindAction("Escape", EIT_OnPressed, &quitApp);

		g_Input->AddActionKeyPairToMap("TurnCamera", EKeyCode::LMB);
		g_Input->AddActionKeyPairToMap("ZoomCamera", EKeyCode::RMB);

		g_Input->AddAxisKeyPairToMap("MoveRight", { EKeyCode::A,EKeyCode::D });
		g_Input->AddAxisKeyPairToMap("MoveForward", { EKeyCode::S,EKeyCode::W });
		g_Input->AddAxisKeyPairToMap("MoveUp", { EKeyCode::Control,EKeyCode::Space });

	}
	void window_focus_callback(GLFWwindow* window, int focused)
	{
		if (g_Input)
		{
			g_Input->bAppInFocus = focused;
		}
	}
	void window_iconify_callback(GLFWwindow* window, int iconified)
	{
		bWindowIconified = iconified > 0;
	}
	void quitApp()
	{
		glfwSetWindowShouldClose(g_Window, true);
	}
	void cleanupInput()
	{
		safe_delete(g_Input);
	}

	void initCamera()
	{
		g_Camera = DBG_NEW cCamera(glm::vec3(0, 1, 2.5), 15.f, 0.0f, 2.0f, 0.1f);
		g_Camera->UpdateProjectionMatrix(glm::radians(45.f), (float)WIDTH / (float)HEIGHT);
		g_Camera->Update();

		g_Input->BindAction("TurnCamera", UserInput::EIT_OnHold, g_Camera, &cCamera::TurnCamera);
		g_Input->BindAction("ZoomCamera", UserInput::EIT_OnHold, g_Camera, &cCamera::ZoomCamera);

		g_Input->BindAxis("MoveRight", g_Camera, &cCamera::MoveRight);
		g_Input->BindAxis("MoveForward", g_Camera, &cCamera::MoveForward);
		g_Input->BindAxis("MoveUp", g_Camera, &cCamera::MoveUp);

	}
	void cleanupCamera()
	{
		safe_delete(g_Camera);
	}

	void check_vk_result(VkResult err)
	{
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	void initImgui()
	{
		assert(g_Window != nullptr && g_Renderer != nullptr);
		if (!g_Window)
		{
			printf("GLFW Window is not initialized before trying initializing imgui\n");
			return;
		}
		if (!g_Renderer)
		{
			printf("VkRenderer Window is not initialized before trying initializing imgui\n");
			return;
		}
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Assign data from VKE to imgui window data
		const FMainDevice& MainDevice = g_Renderer->GetMainDevice();
		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		// Basic info from devices
		wd->Width = WIDTH;
		wd->Height = HEIGHT;
		wd->Swapchain = g_Renderer->GetSwapChain().SwapChain;
		wd->Surface = g_Renderer->GetSurface();
		wd->SurfaceFormat = g_Renderer->GetSwapChainDetail().getSurfaceFormat();
		wd->PresentMode = g_Renderer->GetSwapChainDetail().getPresentationMode();	
		// Set up clearing value
		wd->ClearEnable = false;
		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		memcpy(&wd->ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));
		wd->FrameIndex = 0;		// would change during rendering
		wd->ImageCount = 3;
		wd->SemaphoreIndex = 0; // would change during rendering
		wd->Frames = DBG_NEW ImGui_ImplVulkanH_Frame[wd->ImageCount];
		wd->FrameSemaphores = DBG_NEW ImGui_ImplVulkanH_FrameSemaphores[wd->ImageCount];
		// Create the Render Pass
		{
			VkAttachmentDescription attachment = {};
			attachment.format = wd->SurfaceFormat.format;
			attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.loadOp = wd->ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			VkAttachmentReference color_attachment = {};
			color_attachment.attachment = 0;
			color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &color_attachment;
			VkSubpassDependency dependency = {};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			VkRenderPassCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			info.attachmentCount = 1;
			info.pAttachments = &attachment;
			info.subpassCount = 1;
			info.pSubpasses = &subpass;
			info.dependencyCount = 1;
			info.pDependencies = &dependency;
			VkResult Result = vkCreateRenderPass(MainDevice.LD, &info, g_Renderer->GetAllocator(), &wd->RenderPass);
			RESULT_CHECK(Result, "Fail to create render pass for imgui");
		}
		// Get VkImages
		std::vector<VkImage> imguiImages(wd->ImageCount, VK_NULL_HANDLE);
		{
			VkResult err = vkGetSwapchainImagesKHR(MainDevice.LD, wd->Swapchain, &wd->ImageCount, imguiImages.data());
			check_vk_result(err);
		}
		std::vector<VkImageView> imguiImageViews(wd->ImageCount, VK_NULL_HANDLE);
		// Create The Image Views
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = wd->SurfaceFormat.format;
			info.components.r = VK_COMPONENT_SWIZZLE_R;
			info.components.g = VK_COMPONENT_SWIZZLE_G;
			info.components.b = VK_COMPONENT_SWIZZLE_B;
			info.components.a = VK_COMPONENT_SWIZZLE_A;
			VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			info.subresourceRange = image_range;
			for (uint32_t i = 0; i < wd->ImageCount; i++)
			{
				info.image = imguiImages[i];
				VkResult err = vkCreateImageView(MainDevice.LD, &info, g_Renderer->GetAllocator(), &imguiImageViews[i]);
				RESULT_CHECK(err, "Fail to create imgui image views");
			}
		}
		
		std::vector<VkFramebuffer> imguiFrameBuffers(wd->ImageCount, VK_NULL_HANDLE);
		// Create Framebuffer
		{
			VkImageView attachment[1];
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = wd->RenderPass;
			info.attachmentCount = 1;
			info.pAttachments = attachment;
			info.width = wd->Width;
			info.height = wd->Height;
			info.layers = 1;
			for (uint32_t i = 0; i < wd->ImageCount; i++)
			{
				attachment[0] = imguiImageViews[i];
				VkResult err = vkCreateFramebuffer(MainDevice.LD, &info, g_Renderer->GetAllocator(), &imguiFrameBuffers[i]);
				check_vk_result(err);
			}
		}
		for (size_t i = 0; i < wd->ImageCount; ++i)
		{
			wd->Frames[i] = 
			{
				MainDevice.GraphicsCommandPool,
				g_Renderer->GetCommandBuffers()[i],
				g_Renderer->GetDrawFences()[i],
				imguiImages[i],
				imguiImageViews[i],
				imguiFrameBuffers[i]
			};
			wd->FrameSemaphores[i] = 
			{
				g_Renderer->GetOnImageAvailables()[i],
				g_Renderer->GetOnRenderFinisheds()[i]
			};
		}

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan(g_Window, true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = g_Renderer->GetvkInstance();
		init_info.PhysicalDevice = MainDevice.PD;
		init_info.Device = MainDevice.LD;
		init_info.QueueFamily = MainDevice.QueueFamilyIndices.graphicFamily;
		init_info.Queue = MainDevice.graphicQueue;
		init_info.PipelineCache = g_Renderer->GetPipelineCache();
		init_info.DescriptorPool = g_Renderer->GetDescriptorPool();
		init_info.Allocator = g_Renderer->GetAllocator();
		init_info.MinImageCount = MAX_FRAME_DRAWS;
		init_info.ImageCount = wd->ImageCount;
		init_info.CheckVkResultFn = check_vk_result;
		// Create descriptor set, pipeline layout, pipeline, needs to be cleanup separately
		ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

		// Upload Fonts
		{
			// Use any command queue
			VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
			VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

			VkResult err = vkResetCommandPool(MainDevice.LD, command_pool, 0);
			check_vk_result(err);
			VkCommandBufferBeginInfo begin_info = {};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			err = vkBeginCommandBuffer(command_buffer, &begin_info);
			check_vk_result(err);

			ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

			VkSubmitInfo end_info = {};
			end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			end_info.commandBufferCount = 1;
			end_info.pCommandBuffers = &command_buffer;
			err = vkEndCommandBuffer(command_buffer);
			check_vk_result(err);
			err = vkQueueSubmit(MainDevice.graphicQueue, 1, &end_info, VK_NULL_HANDLE);
			check_vk_result(err);

			err = vkDeviceWaitIdle(MainDevice.LD);
			check_vk_result(err);
			ImGui_ImplVulkan_DestroyFontUploadObjects();
		}
	}

	void cleanupImgui()
	{
		const FMainDevice& MainDevice = g_Renderer->GetMainDevice();
		// wait until the device is not doing anything (nothing on any queue)
		vkDeviceWaitIdle(MainDevice.LD);
		ImGui_ImplVulkan_Cleanup_External(MainDevice.LD, g_Renderer->GetAllocator());
		
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		// Imgui stuffs
		ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
		for (size_t i = 0; i < wd->ImageCount; ++i)
		{
			vkDestroyFramebuffer(MainDevice.LD, wd->Frames[i].Framebuffer, g_Renderer->GetAllocator());
		}
		vkDestroyRenderPass(MainDevice.LD, wd->RenderPass, g_Renderer->GetAllocator());
		for (size_t i = 0; i < wd->ImageCount; ++i)
		{
			vkDestroyImageView(MainDevice.LD, wd->Frames[i].BackbufferView, g_Renderer->GetAllocator());
		}

		delete[] g_MainWindowData.Frames;
		delete[] g_MainWindowData.FrameSemaphores;
	}
}