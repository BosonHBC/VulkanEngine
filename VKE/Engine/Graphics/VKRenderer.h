#pragma once
// If no GLFW, this macro is required to create a win32 specific surface
//#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "Engine.h"
#include "Utilities.h"

#include "BufferFormats.h"
#include "Descriptors/DescriptorSet.h"
#include "Mesh/Mesh.h"
#include "Buffer/ImageBuffer.h"

#include <vector>
namespace VKE
{
	class cModel;
	struct FComputePass;
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

		bool CreateModel(const std::string& ifileName, cModel*& oModel);
		// Scene Objects
		std::vector<cModel*> RenderList;
	private:
		// GLFW window
		GLFWwindow* window;

		// Compute pass
		FComputePass* pCompute = nullptr;

		uint64_t ElapsedFrame = 0;

		// Vulkan Components
		// - Main Components
		FMainDevice MainDevice;
		VkInstance vkInstance;
		VkSurfaceKHR Surface;								// KHR extension required

		// SwapChainImages, SwapChainFramebuffers, CommandBuffers are all 1 to 1 correspondent
		FSwapChainData SwapChain;							// SwapChain data group
		std::vector<VkFramebuffer> SwapChainFramebuffers;
		std::vector<VkCommandBuffer> CommandBuffers;

		std::vector <cImageBuffer> DepthBuffers;
		std::vector <cImageBuffer> ColorBuffers;			

		// -Pipeline
		VkRenderPass RenderPass;
		
		VkPipeline GraphicPipeline;
		VkPipelineLayout PipelineLayout;
		
		VkPipeline SecondGraphicPipeline;
		VkPipelineLayout SecondPipelineLayout;

		// -Synchronization
		std::vector<VkSemaphore> OnImageAvailables;						// If this image is locked by other usage
		std::vector <VkSemaphore> OnRenderFinisheds;					// If this image finishes rendering
		std::vector<VkFence> DrawFences;								// Fence allow to block the program by ourself

		// - Descriptors
#pragma region Uniform Buffer / Dynamic Descripotor set
		VkDescriptorPool DescriptorPool;
		std::vector<cDescriptorSet> DescriptorSets;
#pragma endregion
		// -- Push Constant
		VkPushConstantRange PushConstantRange;
		// -- Sampler Descriptor Set
		VkDescriptorSetLayout SamplerSetLayout;
		VkDescriptorPool SamplerDescriptorPool;

		// -- Input Descriptor Set
		VkDescriptorPool InputDescriptorPool;
		std::vector<cDescriptorSet> InputDescriptorSets;

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
		void createFrameBufferImage();
		void createFrameBuffer();
		void createCommandPool();
		void createCommandBuffers();
		void createSynchronization();

		void createUniformBuffer();
		void createDescriptorPool();
		void allocateDescriptorSetsAndUpdateDescriptorSetWrites();

		/** intermediate functions */
		void prepareForDraw();
		void recordCommands();
		void updateUniformBuffers();
		void presentFrame();

		/** Support functions */
		bool checkInstanceExtensionSupport(const char** checkExtentions, int extensionCount);
		bool checkValidationLayerSupport();
		bool checkDeviceExtensionSupport(const VkPhysicalDevice& device);
		bool checkDeviceSuitable(const VkPhysicalDevice& device);
		VkFormat chooseSupportedFormat(const std::vector<VkFormat>& Formats, VkImageTiling Tiling, VkFormatFeatureFlags FeatureFlags);

		/** Getters */
		void getQueueFamilies(const VkPhysicalDevice& device);
		FSwapChainDetail getSwapChainDetail(const VkPhysicalDevice& device);
	};

}
