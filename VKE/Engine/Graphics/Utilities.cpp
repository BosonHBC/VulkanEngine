#include "Utilities.h"
#include "../Engine.h"
#include "ComputePass.h"

#include <algorithm>
#include <fstream>
#include <random>
#include "stb_image.h"
namespace VKE
{
	std::default_random_engine RndGenerator { 0 };
	std::uniform_real_distribution<float> Float01Distribution(0.0f, 1.0f);

	VkDeviceSize MinUniformBufferOffset = 0;

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
			int32 width, height;
			glfwGetFramebufferSize(VKE::GetGLFWWindow(), &width, &height);

			VkExtent2D NewExtent;
			// Make new extent is inside boundaries defined by Surface
			NewExtent.width = std::max(std::min(static_cast<uint32_t>(width), SurfaceCapabilities.maxImageExtent.width), SurfaceCapabilities.minImageExtent.width);
			NewExtent.height = std::max(std::min(static_cast<uint32_t>(height), SurfaceCapabilities.maxImageExtent.height), SurfaceCapabilities.minImageExtent.height);

			return NewExtent;
		}
	}


	VkResult FSwapChainData::acquireNextImage(FMainDevice MainDevice, VkSemaphore PresentCompleteSemaphore)
	{
		return vkAcquireNextImageKHR(MainDevice.LD, SwapChain,
			std::numeric_limits<uint64_t>::max(),						// never timeout
			PresentCompleteSemaphore,								// Signal us when that image is available to use
			VK_NULL_HANDLE,
			&ImageIndex);
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

		const VkResult Result = vkCreateShaderModule(LD, &ShaderModuleCreateModule, nullptr, &ShaderModule);

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
		RESULT_CHECK(Result, "Fail to create buffer.")
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

		// Allocate memory to VKDeviceMemory
		Result = vkAllocateMemory(MainDevice.LD, &MemAllocInfo, nullptr, &oBufferMemory);
		RESULT_CHECK(Result, "Fail to allocate memory for buffer.")
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

	VkCommandBuffer BeginCommandBuffer(VkDevice LD, VkCommandPool CommandPool)
	{
		// Get a command buffer from command buffer pool
		VkCommandBuffer CommandBuffer = {};

		VkCommandBufferAllocateInfo AllocInfo = {};
		AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		AllocInfo.commandPool = CommandPool;
		AllocInfo.commandBufferCount = 1;										// Only one command buffer needed

		vkAllocateCommandBuffers(LD, &AllocInfo, &CommandBuffer);

		// Record command in command buffer
		VkCommandBufferBeginInfo CommandBeginInfo = {};
		CommandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		CommandBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;		// We're only using the command buffer once, so set up for one time transfer

		// Begin recording
		vkBeginCommandBuffer(CommandBuffer, &CommandBeginInfo);

		return CommandBuffer;
	}

	void EndCommandBuffer(VkCommandBuffer CommandBuffer, VkDevice LD, VkQueue Queue, VkCommandPool CommandPool)
	{
		// End Recording
		vkEndCommandBuffer(CommandBuffer);

		// Create fence to ensure that the command buffer has finished executing

		VkFenceCreateInfo FenceCreateInfo = {};
		FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceCreateInfo.flags = 0;
		VkFence Fence;
		VkResult Result = vkCreateFence(LD, &FenceCreateInfo, nullptr, &Fence);
		RESULT_CHECK(Result, "Fail to create fence for executing transfer command")

		// Queue submission information
		VkSubmitInfo SubmitInfo = {};
		SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		SubmitInfo.commandBufferCount = 1;
		SubmitInfo.pCommandBuffers = &CommandBuffer;

		Result = vkQueueSubmit(Queue, 1, &SubmitInfo, Fence);
		RESULT_CHECK(Result, "Fail to submit transfer command buffer to transfer queue")

		// Use fence to prevent submitting multiple transfer command buffers to a same queue,
		// Sometime when there are tons of meshes loading in one time, this way can prevent crashing easily,
		// But optimal way is using synchronization to load files at the same time.
		Result = vkWaitForFences(LD, 1, &Fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		RESULT_CHECK(Result, "Fail to wait for fence")
		
		// Free temp fence
		vkDestroyFence(LD, Fence, nullptr);
		// Free temporary command buffer back to pool
		vkFreeCommandBuffers(LD, CommandPool, 1, &CommandBuffer);
	}

	void CopyBuffer(VkDevice LD, VkQueue TransferQueue, VkCommandPool TransferCommandPool, VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize BufferSize)
	{
		// Allocate the transfer command buffer
		const VkCommandBuffer TransferCommandBuffer = BeginCommandBuffer(LD, TransferCommandPool);

		// Region of data to copy from and to, allows copy multiple regions of data
		VkBufferCopy BufferCopyRegion;
		BufferCopyRegion.srcOffset = 0;
		BufferCopyRegion.dstOffset = 0;
		BufferCopyRegion.size = BufferSize;

		// Command to copy src buffer to dst buffer
		vkCmdCopyBuffer(TransferCommandBuffer, SrcBuffer, DstBuffer, 1, &BufferCopyRegion);

		// Stop the command and submit it to the queue, wait until it finish execution
		EndCommandBuffer(TransferCommandBuffer, LD, TransferQueue, TransferCommandPool);
	}

	void CopyImageBuffer(VkDevice LD, VkQueue TransferQueue, VkCommandPool TransferCommandPool, VkBuffer SrcBuffer, VkImage DstImage, uint32_t Width, uint32_t Height)
	{
		const VkCommandBuffer TransferCommandBuffer = BeginCommandBuffer(LD, TransferCommandPool);

		// Region of data to copy from and to, allows copy multiple regions of data
		VkBufferImageCopy ImageCopyRegion;
		ImageCopyRegion.bufferOffset = 0;
		ImageCopyRegion.bufferRowLength = 0;										// for calculating data spacing, like calculating i, j for loop
		ImageCopyRegion.bufferImageHeight = 0;										// the same calculating data spacing, means don't want to skip any pixel while reading
		ImageCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// Which aspect of image to copy
		ImageCopyRegion.imageSubresource.mipLevel = 0;								// Mipmap level to copy
		ImageCopyRegion.imageSubresource.baseArrayLayer = 0;						// Starting array layer
		ImageCopyRegion.imageSubresource.layerCount = 1;							// Number of layers to copy starting at baseArrayLayer
		ImageCopyRegion.imageOffset = { 0,0,0 };									// Offset into image (as opposed to raw data in buffer offset), start at the beginning
		ImageCopyRegion.imageExtent = { Width, Height, 1 };							// Size of region to copy as (x,y,z) values


		// Command to copy src buffer to dst buffer
		vkCmdCopyBufferToImage(TransferCommandBuffer, SrcBuffer, DstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ImageCopyRegion);

		EndCommandBuffer(TransferCommandBuffer, LD, TransferQueue, TransferCommandPool);
	}

	void SetMinUniformOffsetAlignment(VkDeviceSize Size)
	{
		MinUniformBufferOffset = Size;
	}

	VkDeviceSize GetMinUniformOffsetAlignment()
	{
		return MinUniformBufferOffset;
	}

	VkImageView CreateImageViewFromImage(const FMainDevice* iMainDevice, const VkImage& iImage, const VkFormat& iFormat, const VkImageAspectFlags& iAspectFlags)
	{
		VkImageViewCreateInfo ViewCreateInfo = {};
		ViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ViewCreateInfo.image = iImage;
		ViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;						// Type of image (e.g. 2D, 3D, cubemap)
		ViewCreateInfo.format = iFormat;										// Format of the image (e.g. R8G8B8)
		ViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;			// Allows remapping rgba components to other rgba values
		ViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

		// SubResources allows the view to view only a part of am image
		ViewCreateInfo.subresourceRange.aspectMask = iAspectFlags;				// Which aspect of image to view (e.g. VK_IMAGE_ASPECT_COLOR_BIT for view)
		ViewCreateInfo.subresourceRange.baseMipLevel = 0;						// Start mipmap level to view from
		ViewCreateInfo.subresourceRange.levelCount = 1;							// Number of mipmap levels to view
		ViewCreateInfo.subresourceRange.baseArrayLayer = 0;						// Start array level to view from
		ViewCreateInfo.subresourceRange.layerCount = 1;							// Number of array levels to view 

		// Create Image view and return it
		VkImageView ImageView;
		const VkResult Result = vkCreateImageView(iMainDevice->LD, &ViewCreateInfo, nullptr, &ImageView);
		RESULT_CHECK(Result, "Fail to create an Image View")

		return ImageView;
	}

	bool CreateImage(const FMainDevice* iMainDevice, uint32_t Width, uint32_t Height, VkFormat Format, VkImageTiling Tiling, VkImageUsageFlags UseFlags, VkMemoryPropertyFlags PropFlags, VkImage& oImage, VkDeviceMemory& oImageMemory)
	{
		// CREATE IMAGE
		VkImageCreateInfo ImgCreateInfo = {};
		ImgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ImgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		ImgCreateInfo.usage = UseFlags;
		ImgCreateInfo.format = Format;
		ImgCreateInfo.tiling = Tiling;								// How image data should be "tiled" (arranged for optimal)
		ImgCreateInfo.extent.width = Width;
		ImgCreateInfo.extent.height = Height;
		ImgCreateInfo.extent.depth = 1;								// No 3D aspect
		ImgCreateInfo.mipLevels = 1;								// LOD
		ImgCreateInfo.arrayLayers = 1;								// Use for cubemap
		ImgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	// Initial layout of image data on creation
		ImgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;				// For multi-sampling
		ImgCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// whether image can be shared between queues

		VkResult Result = vkCreateImage(iMainDevice->LD, &ImgCreateInfo, nullptr, &oImage);
		RESULT_CHECK(Result, "Fail to create an Image.")
		if (Result != VK_SUCCESS)
		{
			return false;
		}
		// Get memory requirements for a type of image
		VkMemoryRequirements MemoryRequirements;
		vkGetImageMemoryRequirements(iMainDevice->LD, oImage, &MemoryRequirements);

		// CREATE MEMORY FOR IMAGE
		// Allocate memory to buffer
		VkMemoryAllocateInfo MemAllocInfo = {};
		MemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		MemAllocInfo.allocationSize = MemoryRequirements.size;
		MemAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(iMainDevice->PD,
			MemoryRequirements.memoryTypeBits,				// Index of memory type on Physical Device that has required bit flags
			PropFlags										// Memory property, is this local_bit or host_bit or others
		);

		// Allocate memory to VKDeviceMemory
		Result = vkAllocateMemory(iMainDevice->LD, &MemAllocInfo, nullptr, &oImageMemory);
		RESULT_CHECK(Result, "Fail to allocate memory for image.")
		if (Result != VK_SUCCESS)
		{
			return false;
		}

		Result = vkBindImageMemory(iMainDevice->LD, oImage, oImageMemory, 0);
		RESULT_CHECK(Result, "Fail to bind image with memory.")
		if (Result != VK_SUCCESS)
		{
			return false;
		}
		return true;
	}

	void TransitionImageLayout(VkDevice LD, VkQueue Queue, VkCommandPool CommandPool, VkImage Image, VkImageLayout CurrentLayout, VkImageLayout NewLayout)
	{
		const VkCommandBuffer CommandBuffer = BeginCommandBuffer(LD, CommandPool);

		// Pipeline barrier - Image memory barrier
		VkImageMemoryBarrier ImageMemoryBarrier = {};
		ImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		ImageMemoryBarrier.oldLayout = CurrentLayout;
		ImageMemoryBarrier.newLayout = NewLayout;
		ImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;			// Don't bother transferring between queues
		ImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		ImageMemoryBarrier.image = Image;											// Image being accessed and modified as part of barrier
		ImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;	// Aspect of image being altered
		ImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		ImageMemoryBarrier.subresourceRange.levelCount = 1;							// Number of mip-map levels to alter starting from base level
		ImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;						// Number of layers to alter starting from baseArrayLayer
		ImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags SrcStage = 0, DstStage = 0;


		// If transitioning from new image ready to receive data...
		if (CurrentLayout == VK_IMAGE_LAYOUT_UNDEFINED && NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			ImageMemoryBarrier.srcAccessMask = 0;									// Memory access stage transition must happen after: From the very start
			ImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;		// Memory access stage transition must happen before: transfer write stage

			SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;							// On top of any stage
			DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		// If transitioning from transfer destination to shader readable...
		else if (CurrentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			ImageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;		// Memory access stage transition must happen after: Finish doing the transfer (from host_visible to local_bit)
			ImageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;			// Memory access stage transition must happen before: the shader starts to read it

			SrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			DstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;						// Should be ready before fragment shader reading the texture
		}
		vkCmdPipelineBarrier(CommandBuffer,
			SrcStage, DstStage,		// Pipeline stages (match to src and dst access masks)
			0, 						// Dependency flags
			0, nullptr,				// Global Memory Barrier count + data
			0, nullptr,				// Buffer Memory barrier count + data
			1, &ImageMemoryBarrier	// Image Memory Barrier count + data
		);

		EndCommandBuffer(CommandBuffer, LD, Queue, CommandPool);
	}

	int32 RandRangeInt(int32 min, int32 max)
	{
		auto result = static_cast<int32>(RandRange(static_cast<float>(min), static_cast<float>(max) + 1.0f));
		// when RandRange_float returns 1.0, result would be max + 1, clamp that
		if (result > max)
		{
			result = max;
		}
		return result;
	}

	float RandRange(float min, float max)
	{
		return (Rand01() * (max - min) + min);
	}

	glm::vec3 RandRange(glm::vec3 min, glm::vec3 max)
	{
		return {RandRange(min.x, max.x), RandRange(min.y, max.y), RandRange(min.z, max.z)};
	}

	float Rand01()
	{
		return Float01Distribution(RndGenerator);
	}

	namespace FileIO {

		std::vector<char> ReadFile(const std::string& filename)
		{
			// Open stream from given file
			// std::ios::binary tells stream to read file as binary
			// std::ios::ate tells stream to start reading from end of file
			const std::string& Path = filename;// FileIO::RelativePathToAbsolutePath(filename);
			std::ifstream file(Path, std::ios::in | std::ios::binary | std::ios::ate);

			if (!file.is_open())
			{
				char msg[250];
				int32 Len = sprintf_s(msg, "Fail to open the file: [%s]!", Path.c_str());
				throw std::runtime_error(msg);
			}

			// Get current read position and use to resize file buffer
			const size_t FileSize = static_cast<size_t>(file.tellg());
			std::vector<char> FileBuffer(FileSize);

			// Move read position (seek to) the start of the file
			file.seekg(0);

			// Read the file data into the buffer (stream "FileSize" in total)
			file.read(FileBuffer.data(), FileSize);

			file.close();

			return FileBuffer;
		}

		std::string RelativePathToAbsolutePath(const std::string& iRelative)
		{
			std::string result = std::string(_OUT_DIR) + iRelative;
			return result;
		}

		uint8* LoadTextureFile(const std::string& fileName, int32& oWidth, int32& oHeight, VkDeviceSize& oImageSize)
		{
			// Number of channel image uses
			int32 channels = 0;

			const std::string fileLoc = "Content/Textures/" + fileName;
			// Make sure always has 4 channels
			stbi_uc* Data = stbi_load(fileLoc.c_str(), &oWidth, &oHeight, &channels, STBI_rgb_alpha);

			if (!Data)
			{
				const std::string ErrorMsg = "Fail to load texture[" + fileLoc + "]." + stbi_failure_reason();
				printf("%s", ErrorMsg.c_str());
				throw std::runtime_error(ErrorMsg);
			}
			// Calculate image size
			oImageSize = static_cast<uint64>(oWidth) * oHeight * 4;
			return Data;
		}

		void freeLoadedTextureData(uint8* Data)
		{
			stbi_image_free(Data);
		}

	}



	bool FQueueFamilyIndices::IsValid() const
	{
		// If compute pipeline is note required, no need to check compute family
		return (graphicFamily >= 0 && presentationFamily >= 0 && (FComputePass::SComputePipelineRequired ? computeFamily >= 0 : true)); 
	}

	namespace Helpers
	{
		VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits ShaderStage, VkShaderModule ShaderModule, const char* MainFunctionName /*= "main"*/)
		{
			VkPipelineShaderStageCreateInfo ShaderStageCreateInfo = {};
			ShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			ShaderStageCreateInfo.stage = ShaderStage;
			ShaderStageCreateInfo.module = ShaderModule;
			ShaderStageCreateInfo.pName = MainFunctionName;
			return ShaderStageCreateInfo;
		}

		VkSubpassDescription SubPassDescriptionDefault(VkPipelineBindPoint BindPoint)
		{
			VkSubpassDescription SubPassDescription;
			SubPassDescription.pipelineBindPoint = BindPoint; // Pipeline type, (Graphics pipeline, Compute pipeline, RayTracing_NV...)
			SubPassDescription.colorAttachmentCount = 0;
			SubPassDescription.pColorAttachments = nullptr;
			SubPassDescription.pDepthStencilAttachment = nullptr;
			SubPassDescription.inputAttachmentCount = 0;
			SubPassDescription.pInputAttachments = nullptr;
			SubPassDescription.preserveAttachmentCount = 0;
			SubPassDescription.pPreserveAttachments = nullptr;
			SubPassDescription.pResolveAttachments = nullptr;
			SubPassDescription.flags = 0;

			return SubPassDescription;
		}

	}


}
