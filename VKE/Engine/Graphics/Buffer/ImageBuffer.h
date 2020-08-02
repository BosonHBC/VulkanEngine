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
	class cImageBuffer
	{
	public:
		/* Constructors and destructor*/
		cImageBuffer() {}
		virtual ~cImageBuffer() {}
		cImageBuffer(const cImageBuffer& i_other) = delete;
		cImageBuffer(cImageBuffer&& i_other) = default;	// Allow using std::vector
		cImageBuffer& operator = (const cImageBuffer& i_other) = delete;
		cImageBuffer& operator = (cImageBuffer&& i_other) = delete;

		bool init(FMainDevice* iMainDevice, uint32_t Width, uint32_t Height, VkFormat Format, VkImageTiling Tiling, VkImageUsageFlags UseFlags, VkMemoryPropertyFlags PropFlags, VkImageAspectFlags AspectFlags);
		void cleanUp();

		// Getters
		const VkImageView& GetImageView() const { return ImageView; }
		const VkImage&  GetImage() const { return Image; }
		const VkDeviceMemory&  GetImageMemory() const { return Memory; }
	private:
		FMainDevice* pMainDevice;

		// Image format
		VkFormat ImageFormat;
		// Components of an image buffer
		VkImage Image;
		VkDeviceMemory Memory;
		VkImageView ImageView;
	};
}
