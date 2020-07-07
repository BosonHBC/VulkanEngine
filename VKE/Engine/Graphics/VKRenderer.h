#pragma once
// If no GLFW, this macro is required to create a win32 specific surface
//#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include "Utilities.h"
#include "BufferFormats.h"


namespace VKE
{
	class cMesh;
	class VKRenderer
	{
	public:
		VKRenderer() {};
		~VKRenderer() { window = nullptr; };

		VKRenderer(const VKRenderer& other) = delete;
		VKRenderer& operator =(const VKRenderer& other) = delete;

		int init(GLFWwindow* iWindow);
		void tick(float dt);
		void draw();
		void cleanUp();

	private:
		GLFWwindow* window;

		uint64_t ElapsedFrame = 0;
		// Scene Objects
		std::vector<cMesh*> RenderList;
		BufferFormats::FFrame FrameData;


		// Vulkan Components
		// - Main Components
		FMainDevice MainDevice;
		VkInstance vkInstance;
		VkQueue graphicQueue;
		VkQueue presentationQueue;
		VkSurfaceKHR Surface;								// KHR extension required

		// SwapChainImages, SwapChainFramebuffers, CommandBuffers are all 1 to 1 correspondent
		FSwapChainData SwapChain;							// SwapChain data group
		std::vector<VkFramebuffer> SwapChainFramebuffers;	
		std::vector<VkCommandBuffer> CommandBuffers;		

		// -Pipeline
		VkRenderPass RenderPass;
		VkPipeline GraphicPipeline;
		VkPipelineLayout PipelineLayout;

		// -Pools
		VkCommandPool GraphicsCommandPool;					// Command Pool only used for graphic command

		// -Synchronization
		std::vector<VkSemaphore> OnImageAvailables;						// If this image is locked by other usage
		std::vector <VkSemaphore> OnRenderFinisheds;					// If this image finishes rendering
		std::vector<VkFence> DrawFences;								// Fence allow to block the program by ourself

		// - Descriptors
		VkDescriptorSetLayout DescriptorSetLayout;
		VkPushConstantRange PushConstantRange;
		VkDescriptorPool DescriptorPool;
		std::vector<VkDescriptorSet> DescriptorSets;

		std::vector<VkBuffer> UniformBuffers_Frame;
		std::vector<VkDeviceMemory> UniformBufferMemories_Frame;

		std::vector<VkBuffer> DUniformBuffers_Drawcall;					// D at the beginning stands for "Dynamic".
		std::vector<VkDeviceMemory> DUniformBufferMemories_Drawcall;
		BufferFormats::FDrawCall* pDrawcallTransferSpace;

		/** Create functions */
		void createInstance();
		void getPhysicalDevice();
		void createLogicalDevice();
		void createSurface();
		void createSwapChain();
		void createRenderPass();
		void createDescriptorSetLayout();
		void createPushConstantRange();
		void createGraphicsPipeline();
		void createFrameBuffer();
		void createCommandPool();
		void createCommandBuffers();
		void createSynchronization();
		
		void createUniformBuffer();
		void createDescriptorPool();
		void createDescriptorSets();

		void updateUniformBuffers(uint32_t ImageIndex);

		/** Support functions */
		bool checkInstanceExtensionSupport(const char** checkExtentions, int extensionCount);
		bool checkValidationLayerSupport();
		bool checkDeviceExtensionSupport(const VkPhysicalDevice& device);
		bool checkDeviceSuitable(const VkPhysicalDevice& device);
		
		/** -Component Create functions */
		VkImageView CreateImageViewFromImage(const VkImage& iImage, const VkFormat& iFormat, const VkImageAspectFlags& iAspectFlags);

		/** Record functions */
		void recordCommands(uint32_t ImageIndex);

		/** Allocation functions*/
		void allocateDynamicBufferTransferSpace();
	
		/** Getters */
		FQueueFamilyIndices getQueueFamilies(const VkPhysicalDevice& device);
		FSwapChainDetail getSwapChainDetail(const VkPhysicalDevice& device);
	};

}
