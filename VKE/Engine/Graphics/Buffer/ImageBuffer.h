/*
	ImageBuffer is a buffer holding image data
	Can be used to:
	1. storing data of out attachments for further usage
	2. Storing imported texture data
*/
#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace VKE
{
	struct FMainDevice;
	class FImageBuffer
	{
	public:
		/* Constructors and destructor*/
		FImageBuffer() = default;
		virtual ~FImageBuffer() = default;
		FImageBuffer(const FImageBuffer& Other) = delete;
		FImageBuffer(FImageBuffer&& Other) = default;	// Allow using std::vector
		FImageBuffer& operator = (const FImageBuffer& Other) = delete;
		FImageBuffer& operator = (FImageBuffer&& Other) = delete;

		bool Init(FMainDevice* iMainDevice, uint32_t Width, uint32_t Height, VkFormat Format, VkImageTiling Tiling, VkImageUsageFlags UseFlags, VkMemoryPropertyFlags PropFlags, VkImageAspectFlags AspectFlags);
		void CleanUp();

		// Getters
		const VkImageView& GetImageView() const { return ImageView; }
		const VkImage&  GetImage() const { return Image; }
		const VkDeviceMemory&  GetImageMemory() const { return Memory; }
		const VkFormat& GetFormat() const { return ImageFormat; }
	private:
		FMainDevice* pMainDevice = nullptr;

		// Image format
		VkFormat ImageFormat = VK_FORMAT_MAX_ENUM;
		// Components of an image buffer
		VkImage Image = NULL;
		VkDeviceMemory Memory = NULL;
		VkImageView ImageView = NULL;
	};
}
