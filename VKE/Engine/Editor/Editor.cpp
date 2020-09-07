#include "Editor.h"
#include "VKRenderer.h"
#include "Utilities.h"
#include "ComputePass.h"
#include "ParticleSystem/Emitter.h"
#include "Descriptors/Descriptor_Buffer.h"
// System
#include "stdio.h"
#include "assert.h"
// imgui
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
namespace VKE
{
	namespace Editor
	{
		bool GShow_demo_window = false;

		// Imgui
		ImGui_ImplVulkanH_Window g_MainWindowData;

		void Update(VKRenderer* Renderer)
		{
			// Start the Dear ImGui frame
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			FComputePass* CP = Renderer->pCompute;

			// Imgui windows

				// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
			if (GShow_demo_window)
				ImGui::ShowDemoWindow(&GShow_demo_window);

			// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
			{
				static float f = 0.0f;
				static int counter = 0;

				ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

				ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
				ImGui::Checkbox("Demo Window", &GShow_demo_window);      // Edit bool storing our window open/close state
				//ImGui::Checkbox("Another Window", &show_another_window);

				if (ImGui::SliderFloat("float", &CP->Emitters[0].EmitterData.Angle, 0.0f, glm::radians(90.f)))
				{
					CP->Emitters[0].bNeedUpdate = true;
					CP->Emitters[0].UpdateEmitterData(CP->Emitters[0].ComputeDescriptorSet.GetDescriptorAt<cDescriptor_Buffer>(2));
				}
				//ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

				if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
					counter++;
				ImGui::SameLine();
				ImGui::Text("counter = %d", counter);

				ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
				ImGui::End();
			}

			

		}

		void Init(GLFWwindow* Window, VKRenderer* Renderer)
		{
			assert(Window != nullptr && Renderer != nullptr);
			if (!Window)
			{
				printf("GLFW Window is not initialized before trying initializing imgui\n");
				return;
			}
			if (!Renderer)
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
			const FMainDevice& MainDevice = Renderer->GetMainDevice();
			ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
			// Basic info from devices
			wd->Width = WIDTH;
			wd->Height = HEIGHT;
			wd->Swapchain = Renderer->GetSwapChain().SwapChain;
			wd->Surface = Renderer->GetSurface();
			wd->SurfaceFormat = Renderer->GetSwapChainDetail().getSurfaceFormat();
			wd->PresentMode = Renderer->GetSwapChainDetail().getPresentationMode();
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
				VkResult Result = vkCreateRenderPass(MainDevice.LD, &info, Renderer->GetAllocator(), &wd->RenderPass);
				RESULT_CHECK(Result, "Fail to create render pass for imgui");
			}
			// Get VkImages
			std::vector<VkImage> imguiImages(wd->ImageCount, VK_NULL_HANDLE);
			{
				VkResult Result = vkGetSwapchainImagesKHR(MainDevice.LD, wd->Swapchain, &wd->ImageCount, imguiImages.data());
				RESULT_CHECK(Result, "Fail to get Imgui swap chain image KHR.\n");
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
					VkResult Result = vkCreateImageView(MainDevice.LD, &info, Renderer->GetAllocator(), &imguiImageViews[i]);
					RESULT_CHECK(Result, "Fail to create Imgui image views.\n");
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
					VkResult Result = vkCreateFramebuffer(MainDevice.LD, &info, Renderer->GetAllocator(), &imguiFrameBuffers[i]);
					RESULT_CHECK(Result, "Fail to create Imgui Framebuffer.\n");
				}
			}
			for (size_t i = 0; i < wd->ImageCount; ++i)
			{
				wd->Frames[i] =
				{
					MainDevice.GraphicsCommandPool,
					Renderer->GetCommandBuffers()[i],
					Renderer->GetDrawFences()[i],
					imguiImages[i],
					imguiImageViews[i],
					imguiFrameBuffers[i]
				};
				wd->FrameSemaphores[i] =
				{
					Renderer->GetOnImageAvailables()[i],
					Renderer->GetOnRenderFinisheds()[i]
				};
			}

			// Setup Platform/Renderer bindings
			ImGui_ImplGlfw_InitForVulkan(Window, true);
			ImGui_ImplVulkan_InitInfo init_info = {};
			init_info.Instance = Renderer->GetvkInstance();
			init_info.PhysicalDevice = MainDevice.PD;
			init_info.Device = MainDevice.LD;
			init_info.QueueFamily = MainDevice.QueueFamilyIndices.graphicFamily;
			init_info.Queue = MainDevice.graphicQueue;
			init_info.PipelineCache = Renderer->GetPipelineCache();
			init_info.DescriptorPool = Renderer->GetDescriptorPool();
			init_info.Allocator = Renderer->GetAllocator();
			init_info.MinImageCount = MAX_FRAME_DRAWS;
			init_info.ImageCount = wd->ImageCount;
			// Create descriptor set, pipeline layout, pipeline, needs to be cleanup separately
			ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

			// Upload Fonts
			{
				// Use any command queue
				VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
				VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

				VkResult Result = vkResetCommandPool(MainDevice.LD, command_pool, 0);
				RESULT_CHECK(Result, "Fail to reset Imgui Command pool.\n");
				VkCommandBufferBeginInfo begin_info = {};
				begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				Result = vkBeginCommandBuffer(command_buffer, &begin_info);
				RESULT_CHECK(Result, "Fail to begin Imgui Command buffer when uploading fonts.\n");

				ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

				VkSubmitInfo end_info = {};
				end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				end_info.commandBufferCount = 1;
				end_info.pCommandBuffers = &command_buffer;
				Result = vkEndCommandBuffer(command_buffer);
				RESULT_CHECK(Result, "Fail to end Imgui Command buffer when uploading fonts.\n");
				Result = vkQueueSubmit(MainDevice.graphicQueue, 1, &end_info, VK_NULL_HANDLE);
				RESULT_CHECK(Result, "Fail to submit Imgui queue when uploading fonts.\n");

				Result = vkDeviceWaitIdle(MainDevice.LD);
				RESULT_CHECK(Result, "Fail to wait for queue to idle when uploading fonts.\n");
				ImGui_ImplVulkan_DestroyFontUploadObjects();
			}
		}

		void CleanUp(VKRenderer* Renderer)
		{
			const FMainDevice& MainDevice = Renderer->GetMainDevice();
			// wait until the device is not doing anything (nothing on any queue)
			vkDeviceWaitIdle(MainDevice.LD);
			ImGui_ImplVulkan_Cleanup_External(MainDevice.LD, Renderer->GetAllocator());

			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();

			// Imgui stuffs
			ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
			for (size_t i = 0; i < wd->ImageCount; ++i)
			{
				vkDestroyFramebuffer(MainDevice.LD, wd->Frames[i].Framebuffer, Renderer->GetAllocator());
			}
			vkDestroyRenderPass(MainDevice.LD, wd->RenderPass, Renderer->GetAllocator());
			for (size_t i = 0; i < wd->ImageCount; ++i)
			{
				vkDestroyImageView(MainDevice.LD, wd->Frames[i].BackbufferView, Renderer->GetAllocator());
			}

			delete[] g_MainWindowData.Frames;
			delete[] g_MainWindowData.FrameSemaphores;
		}

		ImGui_ImplVulkanH_Window* GetMainWindowData()
		{
			return &g_MainWindowData;
		}

	}
}