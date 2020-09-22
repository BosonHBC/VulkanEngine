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

		bool CreateModel(const std::string& ifileName, std::shared_ptr<cModel>& oModel);
		// Scene Objects
		std::vector<std::shared_ptr<cModel>> RenderList;
	
		// Accessors
		ACCESSOR_INLINE(VkInstance, vkInstance);
		ACCESSOR_INLINE(VkSurfaceKHR, Surface);
		ACCESSOR_INLINE(FMainDevice, MainDevice);
		ACCESSOR_INLINE(FSwapChainData, SwapChain);
		ACCESSOR_INLINE(FSwapChainDetail, SwapChainDetail);
		ACCESSOR_INLINE(VkPipelineCache, PipelineCache);
		ACCESSOR_INLINE(VkDescriptorPool, DescriptorPool);
		ACCESSOR_PTR_INLINE(VkAllocationCallbacks, Allocator);
		ACCESSOR_INLINE(std::vector<VkFramebuffer>, SwapChainFramebuffers);
		ACCESSOR_INLINE(std::vector<VkCommandBuffer>, CommandBuffers);
		ACCESSOR_INLINE(std::vector<VkSemaphore>, OnImageAvailables);
		ACCESSOR_INLINE(std::vector <VkSemaphore>, OnRenderFinisheds);
		ACCESSOR_INLINE(std::vector<VkFence>, DrawFences);
		ACCESSOR_INLINE(std::vector <cImageBuffer>, ColorBuffers);

		// Compute pass
		FComputePass* pCompute = nullptr;
	private:
		// GLFW window
		GLFWwindow* window;

		// Custom Allocator
		VkAllocationCallbacks*   Allocator = nullptr;

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

		VkPipelineCache PipelineCache = VK_NULL_HANDLE;
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
		// First pass
		VkDescriptorPool DescriptorPool;
		std::vector<cDescriptorSet> DescriptorSets;

		// -- Push Constant
		VkPushConstantRange PushConstantRange;
		// -- Sampler Descriptor Set
		VkDescriptorPool SamplerDescriptorPool;

		// -- Input Descriptor Set
		// Third pass
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
