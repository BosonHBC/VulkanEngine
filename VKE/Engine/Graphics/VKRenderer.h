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

		void LoadAssets();

		bool CreateModel(const std::string& ifileName, cModel*& oModel);
		// Scene Objects
		std::vector<cModel*> RenderList;
	private:
		// GLFW window
		GLFWwindow* window;

		// Compute pass
		FComputePass* pCompute = nullptr;

		// Vulkan Components
		// - Main Components
		FMainDevice MainDevice;
		VkInstance vkInstance;
		VkSurfaceKHR Surface;								// KHR extension required

		// SwapChainImages, SwapChainFramebuffers, CommandBuffers are all 1 to 1 correspondent
		FSwapChainDetail SwapChainDetail;
		FSwapChainData SwapChain;							// SwapChain data group
		std::vector<VkFramebuffer> SwapChainFramebuffers;
		std::vector<VkCommandBuffer> CommandBuffers;

		std::vector <cImageBuffer> DepthBuffers;
		std::vector <cImageBuffer> ColorBuffers;			

		// -Pipeline
		VkRenderPass RenderPass;
		
		// first pass
		VkPipeline GraphicPipeline;
		VkPipelineLayout PipelineLayout;
		
		// second pass
		VkPipeline RenderParticlePipeline;
		VkPipelineLayout RenderParticlePipelineLayout;

		// third pass
		VkPipeline PostProcessPipeline;
		VkPipelineLayout PostProcessPipelineLayout;

		// -Synchronization
		std::vector<VkSemaphore> OnGraphicFinished;						// Used for graphic/compute sync
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
		VkDescriptorPool SamplerDescriptorPool;

		// -- Input Descriptor Set
		std::vector<cDescriptorSet> InputDescriptorSets;

		bool bMinimizing = false;

		/** Create functions */
		void createInstance();
		void getPhysicalDevice();
		void createLogicalDevice();

		void createSurface();
		void createSwapChain();
		void createRenderPass();

		void CreateDescriptorSets();
		void createDescriptorPool();
		void createPushConstantRange();

		void createFrameBufferImage();
		void createFrameBuffer();
		void createCommandPool();
		void createCommandBuffers();
		void createSynchronization();
		void createGraphicsPipeline();

		/** Handle SwapChain recreation*/
		void recreateSwapChain();
		void cleanupSwapChain();

		/** intermediate functions */
		VkResult prepareForDraw();
		void recordCommands();
		void updateUniformBuffers();
		VkResult presentFrame();
		void postPresentationStage();
		/** Support functions */
		bool checkInstanceExtensionSupport(const char** checkExtentions, int extensionCount);
		bool checkValidationLayerSupport();
		bool checkDeviceExtensionSupport(const VkPhysicalDevice& device);
		bool checkDeviceSuitable(const VkPhysicalDevice& device);
		VkFormat chooseSupportedFormat(const std::vector<VkFormat>& Formats, VkImageTiling Tiling, VkFormatFeatureFlags FeatureFlags);

		/** Getters */
		void getQueueFamilies(const VkPhysicalDevice& device);
		void getSwapChainDetail(const VkPhysicalDevice& device);
	};

}
