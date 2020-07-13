#pragma once
// If no GLFW, this macro is required to create a win32 specific surface
//#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include "Utilities.h"
#include "BufferFormats.h"
#include "Descriptors/Descriptor_Dynamic.h"
#include "Mesh/Mesh.h"

namespace VKE
{
	class cModel;
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
		BufferFormats::FFrame FrameData;
	private:
		GLFWwindow* window;

		uint64_t ElapsedFrame = 0;

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

		VkFormat DepthFormat;
		VkImage DepthBufferImage;
		VkDeviceMemory DepthBufferImageMemory;
		VkImageView DepthBufferImageView;
		
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
#pragma region Uniform Buffer / Dynamic Descripotor set
		VkDescriptorSetLayout DescriptorSetLayout;
		VkDescriptorPool DescriptorPool;
		std::vector<VkDescriptorSet> DescriptorSets;
		std::vector<cDescriptor> Descriptor_Frame;
		std::vector<cDescriptor_Dynamic> Descriptor_Drawcall;
#pragma endregion
		// -- Push Constant
		VkPushConstantRange PushConstantRange;
		// -- Sampler Descriptor Set
		VkDescriptorSetLayout SamplerSetLayout;
		VkDescriptorPool SamplerDescriptorPool;
		VkSampler TextureSampler;

		// - Assets
		std::vector<VkImage> TextureImages;
		std::vector<VkDeviceMemory> TextureImageMemories;					// All texture should be in one memory with offset, but simplicity, multiple memories are used here
		std::vector<VkImageView> TextureImageViews;
		std::vector<VkDescriptorSet> SamplerDescriptorSets;					// Each image needs a descriptor (sampler)

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
		void createDepthBufferImage();
		void createFrameBuffer();
		void createCommandPool();
		void createCommandBuffers();
		void createSynchronization();
		void createTextureSampler();
		
		void createUniformBuffer();
		void createDescriptorPool();
		void createDescriptorSets();

		void updateUniformBuffers(uint32_t ImageIndex);

		/** Support functions */
		bool checkInstanceExtensionSupport(const char** checkExtentions, int extensionCount);
		bool checkValidationLayerSupport();
		bool checkDeviceExtensionSupport(const VkPhysicalDevice& device);
		bool checkDeviceSuitable(const VkPhysicalDevice& device);
		VkFormat chooseSupportedFormat(const std::vector<VkFormat>& Formats, VkImageTiling Tiling, VkFormatFeatureFlags FeatureFlags);
		/** -Component Create functions */
		VkImageView CreateImageViewFromImage(const VkImage& iImage, const VkFormat& iFormat, const VkImageAspectFlags& iAspectFlags);
		bool createImage(uint32_t Width, uint32_t Height, VkFormat Format, VkImageTiling Tiling,VkImageUsageFlags UseFlags, VkMemoryPropertyFlags PropFlags, VkImage& oImage, VkDeviceMemory& oImageMemory);
		int createTextureImage(const std::string& fileName);
		int createTexture(const std::string& fileName);
		int createTextureDescriptor(VkImageView TextureImage, VkImageView NormalImage);

		/** Record functions */
		void recordCommands(uint32_t ImageIndex);

		/** Getters */
		FQueueFamilyIndices getQueueFamilies(const VkPhysicalDevice& device);
		FSwapChainDetail getSwapChainDetail(const VkPhysicalDevice& device);
	};

}
