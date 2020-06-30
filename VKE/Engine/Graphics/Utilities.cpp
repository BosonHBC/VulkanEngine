#include "Utilities.h"
#include "../Engine.h"
#include <algorithm>
#include <fstream>

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

	std::vector<char> ReadFile(const std::string& filename)
	{
		// Open stream from given file
		// std::ios::binary tells stream to read file as binary
		// std::ios::ate tells stream to start reading from end of file
		std::string Path = filename;// FileIO::RelativePathToAbsolutePath(filename);
		std::ifstream file(Path, std::ios::in | std::ios::binary | std::ios::ate);

		if (!file.is_open())
		{
			char msg[250];
			sprintf_s(msg, "Fail to open the file: [%s]!", Path.c_str());
			throw std::runtime_error(msg);
		}

		// Get current read position and use to resize file buffer
		size_t FileSize = static_cast<size_t>(file.tellg());
		std::vector<char> FileBuffer(FileSize);

		// Move read position (seek to) the start of the file
		file.seekg(0);

		// Read the file data into the buffer (stream "FileSize" in total)
		file.read(FileBuffer.data(), FileSize);

		file.close();

		return FileBuffer;
	}

	FShaderModuleScopeGuard::~FShaderModuleScopeGuard()
	{
		// Destroy Shader Modules when the scoped is expired
		if (LogicDevice != nullptr && Valid)
		{
			vkDestroyShaderModule(LogicDevice, ShaderModule, nullptr);
		}
		
	}

	bool FShaderModuleScopeGuard::CreateShaderModule(const VkDevice& LD, const std::vector<char>& iShaderCode)
	{
		LogicDevice = LD;

		VkShaderModuleCreateInfo ShaderModuleCreateModule = {};
		ShaderModuleCreateModule.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		ShaderModuleCreateModule.codeSize = iShaderCode.size();										// Length of code
		ShaderModuleCreateModule.pCode = reinterpret_cast<const uint32_t*>(iShaderCode.data());		// Pointer to code

		VkResult Result = vkCreateShaderModule(LD, &ShaderModuleCreateModule, nullptr, &ShaderModule);

		if (Result != VK_SUCCESS)
		{
			return false;
		}
		Valid = true;
		return true;
	}

	std::string FileIO::RelativePathToAbsolutePath(const std::string& iReleative)
	{
		std::string result = std::string(_OUT_DIR) + iReleative.c_str();
		return result;
	}

}
