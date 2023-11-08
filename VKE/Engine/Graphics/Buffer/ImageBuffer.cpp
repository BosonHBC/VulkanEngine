#include "ImageBuffer.h"
#include "Utilities.h"

namespace VKE
{

	bool FImageBuffer::Init(FMainDevice* iMainDevice, uint32_t Width, uint32_t Height, VkFormat Format, VkImageTiling Tiling,  VkImageUsageFlags UseFlags, VkMemoryPropertyFlags PropFlags, VkImageAspectFlags AspectFlags)
	{
		pMainDevice = iMainDevice;
		if (pMainDevice == nullptr)
		{
			printf("cImageBuffer::init(...): pMainDevice is nullptr, return false\n");
			return false;
		}
		
		ImageFormat = Format;
		if (!CreateImage(pMainDevice, Width, Height, ImageFormat, Tiling, UseFlags, PropFlags, Image, Memory))
		{
			return false;
		}

		ImageView = CreateImageViewFromImage(pMainDevice, Image, ImageFormat, AspectFlags);
		return true;
	}

	void FImageBuffer::CleanUp()
	{
		if (pMainDevice)
		{
			vkDestroyImageView(pMainDevice->LD, ImageView, nullptr);
			vkDestroyImage(pMainDevice->LD, Image, nullptr);
			vkFreeMemory(pMainDevice->LD, Memory, nullptr);
		}

	}

}