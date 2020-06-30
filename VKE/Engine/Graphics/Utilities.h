#pragma once
#include <vector>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

// Indices (location) of Queue Families (if they exist at all)
namespace VKE
{
	// ================================================
	// =============== Global Variables =============== 
	// ================================================

	// Device Extensions' name
	const std::vector<const char*> DeviceExtensions = 
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	// =======================================
	// =============== Structs =============== 
	// =======================================
	struct FQueueFamilyIndices
	{
		int graphicFamily = -1;			// Location of Graphics Queue Family
		int presentationFamily = -1;	// Location of Presentation Queue Family

		bool IsValid() const { return (graphicFamily >= 0 && presentationFamily >= 0); }
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

	// ================================================
	// =============== Global Functions =============== 
	// ================================================
	std::vector<char> ReadFile(const std::string& filename);

	namespace FileIO
	{
		std::string RelativePathToAbsolutePath(const std::string& iReleative);
		
	}
}
