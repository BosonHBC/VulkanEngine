#pragma once
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#ifdef _DEBUG
#define RESULT_CHECK(Result, Message)					\
{														\
	if (Result != VK_SUCCESS)							\
	{													\
		throw std::runtime_error(Message);				\
		assert(false);									\
	}													\
}
#else
#define RESULT_CHECK(Result, Message) if(Result != VK_SUCCESS) {printf(Message);}
#endif // _DEBUG

#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#ifdef _DEBUG
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#else
#define DBG_NEW new
#endif

#define RESULT_CHECK_ARGS(Result, Message, Args) if(Result != VK_SUCCESS) {char Msg[256]; sprintf_s(Msg, Message, Args); throw std::runtime_error(Msg);}
#define safe_delete(x) if(x!=nullptr) {delete x; x = nullptr; }

namespace VKE
{
	// ================================================
	// =============== Global Variables =============== 
	// ================================================
	const std::vector<const char*> ValidationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef NDEBUG
	const bool EnableValidationLayers = true;
#else
	const bool EnableValidationLayers = true;
#endif
	// Device Extensions' name
	const std::vector<const char*> DeviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	// Maximum 2 image on the queue
	const int MAX_FRAME_DRAWS = 2;
	// Max objects are allowed in the scene
	const int MAX_OBJECTS = 20;
	

	// =======================================
	// =============== Structs =============== 
	// =======================================

	// Vertex data
	struct FVertex
	{
		glm::vec3 Position;		// Vertex Position (x,y,z)
		glm::vec3 Color;		// Vertex color
		glm::vec2 TexCoord;		// Texture coordinate
	};

	// Indices (location) of Queue Families (if they exist at all)
	struct FQueueFamilyIndices
	{
		int graphicFamily = -1;			// Location of Graphics Queue Family
		int presentationFamily = -1;	// Location of Presentation Queue Family
		int computeFamily = -1;			// Location of compute queue family
		bool IsValid() const;
	};

	struct FMainDevice
	{
		VkPhysicalDevice PD;					// Physical Device
		VkDevice LD;							// Logical Device
		VkQueue graphicQueue;					// Graphic Queue,also transfer queue
		VkQueue presentationQueue;				// Presentation Queue
		FQueueFamilyIndices QueueFamilyIndices;		// Queue families
		VkCommandPool GraphicsCommandPool;		// Command Pool only used for graphic command
	};



	struct FSwapChainDetail
	{
		VkSurfaceCapabilitiesKHR SurfaceCapabilities;			// Surface properties, e.g. image size / extents
		std::vector< VkSurfaceFormatKHR> ImgFormats;			// Image formats, e.g. rgb, rgba, r16g16b16a16
		std::vector<VkPresentModeKHR> PresentationModes;		// How image should be presented to screen


		bool IsValid() const { return ImgFormats.size() > 0 && PresentationModes.size() > 0; }
		VkSurfaceFormatKHR getSurfaceFormat() const;
		VkPresentModeKHR getPresentationMode() const;
		VkExtent2D getSwapExtent() const;
	};

	struct FSwapChainImage
	{
		VkImage Image;										// Raw data of the image, can not use it directly
		VkImageView ImgView;								// Describes how to read an image (2D or 3D addresses), and what part of the image to read (color channels, mipmap levels, etc.)
	};

	struct FSwapChainData
	{
		std::vector<FSwapChainImage> Images;		// Array of uint64_t * 2
		VkSwapchainKHR SwapChain;							// uint64_t
		VkExtent2D Extent;									// uint32_t* 2
		VkFormat ImageFormat;								// enum
		uint32_t ImageIndex;						// current updating framebuffer index, also the index of next image to be drawn

		void acquireNextImage(FMainDevice MainDevice, VkSemaphore PresentCompleteSemaphore);
	};

	struct FShaderModuleScopeGuard
	{
		FShaderModuleScopeGuard() {};
		FShaderModuleScopeGuard(const FShaderModuleScopeGuard& iOther) = delete;
		FShaderModuleScopeGuard& operator = (const FShaderModuleScopeGuard& iOther) = delete;
		~FShaderModuleScopeGuard();

		bool CreateShaderModule(const VkDevice& LD, const std::vector<char>& iShaderCode);
		VkShaderModule ShaderModule;
		VkDevice LogicDevice;
		bool Valid = false;
	};

	// =======================================================================
	// =============== Create Info generation helper Functions =============== 
	// =======================================================================

	namespace Helpers
	{
		VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits ShaderStage, VkShaderModule ShaderModule, const char* MainFunctionName = "main");
	}

	// ================================================
	// =============== Global Functions =============== 
	// ================================================

	// Create buffer and allocate memory for any specific usage type of buffer
	bool CreateBufferAndAllocateMemory(FMainDevice MainDevice, VkDeviceSize BufferSize, VkBufferUsageFlags Flags, VkMemoryPropertyFlags Properties, VkBuffer& oBuffer, VkDeviceMemory& oBufferMemory);
	// Find a valid memory type index
	uint32_t FindMemoryTypeIndex(VkPhysicalDevice PD, uint32_t AllowedTypes, VkMemoryPropertyFlags Properties);
	
	VkCommandBuffer BeginCommandBuffer(VkDevice LD, VkCommandPool CommandPool);
	void EndCommandBuffer(VkCommandBuffer CommandBuffer, VkDevice LD, VkQueue Queue, VkCommandPool CommandPool);

	// Copy data from src buffer to dst buffer
	void CopyBuffer(VkDevice LD, VkQueue TransferQueue, VkCommandPool TransferCommandPool,
		VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize BufferSize);

	// Copy Image buffer
	void CopyImageBuffer(VkDevice LD, VkQueue TransferQueue, VkCommandPool TransferCommandPool,
		VkBuffer SrcBuffer, VkImage DstImage, uint32_t Width, uint32_t Height);

	// Getter and setter for MinUniformOffsetAlignment
	void SetMinUniformOffsetAlignment(VkDeviceSize Size);
	VkDeviceSize GetMinUniformOffsetAlignment();

	// Image creation related
	VkImageView CreateImageViewFromImage(FMainDevice* iMainDevice, const VkImage& iImage, const VkFormat& iFormat, const VkImageAspectFlags& iAspectFlags);
	bool CreateImage(FMainDevice* iMainDevice, uint32_t Width, uint32_t Height, VkFormat Format, VkImageTiling Tiling, VkImageUsageFlags UseFlags, VkMemoryPropertyFlags PropFlags, VkImage& oImage, VkDeviceMemory& oImageMemory);

	void TransitionImageLayout(VkDevice LD, VkQueue Queue, VkCommandPool CommandPool, VkImage Image, VkImageLayout CurrentLayout, VkImageLayout NewLayout);

	namespace FileIO
	{
		std::vector<char> ReadFile(const std::string& filename);
		std::string RelativePathToAbsolutePath(const std::string& iReleative);

		unsigned char* LoadTextureFile(const std::string& fileName, int& oWidth, int& oHeight, VkDeviceSize& oImageSize);
		void freeLoadedTextureData(unsigned char* Data);
	}

	float RandRange(float min, float max);
	glm::vec3 RandRange(glm::vec3 min, glm::vec3 max);
}
