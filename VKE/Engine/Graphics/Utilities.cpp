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
			printf("Fail to create Shader Module.\n");
			return false;
		}
		Valid = true;
		return true;
	}
	bool CreateBufferAndAllocateMemory(FMainDevice MainDevice, VkDeviceSize BufferSize, VkBufferUsageFlags Flags, VkMemoryPropertyFlags Properties, VkBuffer& oBuffer, VkDeviceMemory& oBufferMemory)
	{
		// info to create vertex buffer, not assigning memory
		VkBufferCreateInfo BufferCreateInfo = {};
		BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		BufferCreateInfo.size = BufferSize;
		BufferCreateInfo.usage = Flags;											// Multiple types of buffer possible, vertex buffer here
		BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;					// only one using in a time, similar to swap chain images

		VkResult Result = vkCreateBuffer(MainDevice.LD, &BufferCreateInfo, nullptr, &oBuffer);
		RESULT_CHECK(Result, "Fail to create buffer.");
		if (Result != VK_SUCCESS)
		{
			return false;
		}

		// Get buffer memory requirements
		VkMemoryRequirements MemRequirements;
		vkGetBufferMemoryRequirements(MainDevice.LD, oBuffer, &MemRequirements);

		// Allocate memory to buffer
		VkMemoryAllocateInfo MemAllocInfo = {};
		MemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		MemAllocInfo.allocationSize = MemRequirements.size;
		MemAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(
			MainDevice.PD,
			MemRequirements.memoryTypeBits,				// Index of memory type on Physical Device that has required bit flags
			Properties									// Memory property, is this local_bit or host_bit or others
			);

		// Allocate memory to VKDevieMemory
		Result = vkAllocateMemory(MainDevice.LD, &MemAllocInfo, nullptr, &oBufferMemory);
		RESULT_CHECK(Result, "Fail to allocate memory for buffer.");
		if (Result != VK_SUCCESS)
		{
			return false;
		}

		// Bind memory with given vertex buffer, need to free memory
		vkBindBufferMemory(MainDevice.LD, oBuffer, oBufferMemory, 0);

		return true;
	}

	uint32_t FindMemoryTypeIndex(VkPhysicalDevice PD, uint32_t AllowedTypes, VkMemoryPropertyFlags Properties)
	{
		// Get properties of physical device memory
		VkPhysicalDeviceMemoryProperties MemProperties;
		vkGetPhysicalDeviceMemoryProperties(PD, &MemProperties);

		for (uint32_t i = 0; i < MemProperties.memoryTypeCount; ++i)
		{
			if ((AllowedTypes & (1 << i))														// Index of memory type must match corresponding bit in allowed Types
				&& ((MemProperties.memoryTypes[i].propertyFlags & Properties) == Properties))	// Desired property bit flags are part of memory type's property flags
			{
				// This memory type is valid, return its index
				return i;
			}
		}
		return static_cast<uint32_t>(-1);
	}

	void CopyBuffer(VkDevice LD, VkQueue TransferQueue, VkCommandPool TransferCommandPool, VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize BufferSize)
	{
		// Get a command buffer from command buffer pool
		VkCommandBuffer TransferCommandBuffer = {};

		VkCommandBufferAllocateInfo AllocInfo = {};
		AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		AllocInfo.commandPool = TransferCommandPool;
		AllocInfo.commandBufferCount = 1;										// Only one command buffer needed

		vkAllocateCommandBuffers(LD, &AllocInfo, &TransferCommandBuffer);

		// Record command in command buffer
		VkCommandBufferBeginInfo CommandBeginInfo = {};
		CommandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		CommandBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		// We're only using the command buffer once, so set up for one time transfer
		
		// Begin recording
		vkBeginCommandBuffer(TransferCommandBuffer, &CommandBeginInfo);
		
		// Region of data to copy from and to, allows copy multiple regions of data
		VkBufferCopy BufferCopyRegion = {};
		BufferCopyRegion.srcOffset = 0;
		BufferCopyRegion.dstOffset = 0;
		BufferCopyRegion.size = BufferSize;

		// Command to copy src buffer to dst buffer
		vkCmdCopyBuffer(TransferCommandBuffer, SrcBuffer, DstBuffer, 1, &BufferCopyRegion);
		
		// End Recording
		vkEndCommandBuffer(TransferCommandBuffer);

		// Queue submission information
		VkSubmitInfo SubmitInfo = {};
		SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		SubmitInfo.commandBufferCount = 1;
		SubmitInfo.pCommandBuffers = &TransferCommandBuffer;

		VkResult Result = vkQueueSubmit(TransferQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
		RESULT_CHECK(Result, "Fail to submit transfer command buffer to transfer queue");
		
		// Prevent submitting multiple transfer command buffers to a same queue,
		// Sometime when there are tons of meshes loading in one time, this way can prevent crashing easily,
		// But optimal way is using synchronization to load files at the same time.
		vkQueueWaitIdle(TransferQueue);

		// Free temporary command buffer back to pool
		vkFreeCommandBuffers(LD, TransferCommandPool, 1, &TransferCommandBuffer);
	}

	namespace FileIO {

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

		std::string RelativePathToAbsolutePath(const std::string& iReleative)
		{
			std::string result = std::string(_OUT_DIR) + iReleative.c_str();
			return result;
		}
	}



}
