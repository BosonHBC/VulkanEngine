#pragma once
// If no GLFW, this macro is required to create a win32 specific surface
//#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include "Utilities.h"

namespace VKE
{
	class VKRenderer
	{
	public:
		VKRenderer() {};
		~VKRenderer() { window = nullptr; };

		VKRenderer(const VKRenderer& other) = delete;
		VKRenderer& operator =(const VKRenderer& other) = delete;

		int init(GLFWwindow* iWindow);
		void cleanUp();



	private:
		GLFWwindow* window;

		// Vulkan Components
		// - Main Components
		struct
		{
			VkPhysicalDevice PD;		// Physical Device
			VkDevice LD;				// Logical Device
		} MainDevice;
		VkInstance vkInstance;
		VkQueue graphicQueue;
		VkQueue presentationQueue;
		VkSurfaceKHR Surface;								// KHR extension required
		FSwapChainData SwapChain;							// SwapChain data group
		std::vector<VkFramebuffer> SwapChainFramebuffers;
		std::vector<VkCommandBuffer> CommandBuffers;

		// -Pipeline
		VkRenderPass RenderPass;
		VkPipeline GraphicPipeline;
		VkPipelineLayout PipelineLayout;

		// -Pools
		VkCommandPool GraphicsCommandPool;					// Command Pool only used for graphic command

		/** Create functions */
		void createInstance();
		void getPhysicalDevice();
		void createLogicalDevice();
		void createSurface();
		void createSwapChain();
		void createRenderPass();
		void createGraphicsPipeline();
		void createFrameBuffer();
		void createCommandPool();
		void createCommandBuffers();

		/** Support functions */
		bool checkInstanceExtensionSupport(const char** checkExtentions, int extensionCount);
		bool checkValidationLayerSupport();
		bool checkDeviceExtensionSupport(const VkPhysicalDevice& device);
		bool checkDeviceSuitable(const VkPhysicalDevice& device);
		
		/** -Component Create functions */
		VkImageView CreateImageViewFromImage(const VkImage& iImage, const VkFormat& iFormat, const VkImageAspectFlags& iAspectFlags);

		/** Record functions */
		void recordCommands();
	
		/** Getters */
		FQueueFamilyIndices getQueueFamilies(const VkPhysicalDevice& device);
		FSwapChainDetail getSwapChainDetail(const VkPhysicalDevice& device);
	};

}
