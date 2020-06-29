#include "Utilities.h"
#include "../Engine.h"
#include <algorithm>
namespace VKE
{

	VkSurfaceFormatKHR FSwapChainDetail::getSurfaceFormat() const
	{
		// All formats are available
		if (ImgFormats.size() == 1 && ImgFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& format : ImgFormats)
		{
			if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return format;
			}
		}

		return ImgFormats[0];
	}


	VkPresentModeKHR FSwapChainDetail::getPresentationMode() const
	{
		for (const auto& mode : PresentationModes)
		{
			if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return mode;
			}
		}

		// This must be available or the system / driver is broken
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D FSwapChainDetail::getSwapExtent() const
	{
		// If the extent is at numeric limits, the extent can vary.
		if (SurfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return SurfaceCapabilities.currentExtent;
		}
		else
		{
			// Need to set extent manually
			int width, height;
			glfwGetFramebufferSize(VKE::GetGLFWWindow(), &width, &height);
			
			VkExtent2D NewExtent = {};
			// Make new extent is inside boundaries defined by Surface
			NewExtent.width = std::max(std::min(static_cast<uint32_t>(width), SurfaceCapabilities.maxImageExtent.width), SurfaceCapabilities.minImageExtent.width);
			NewExtent.height = std::max(std::min(static_cast<uint32_t>(height), SurfaceCapabilities.maxImageExtent.height), SurfaceCapabilities.minImageExtent.height);

			return NewExtent;
		}
	}

}
