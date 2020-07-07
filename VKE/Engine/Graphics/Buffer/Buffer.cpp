#include "Buffer.h"
#include "../Utilities.h"
namespace VKE
{

	bool cBuffer::CreateBufferAndAllocateMemory(VkPhysicalDevice PD, VkDevice LD, VkDeviceSize BufferSize, VkBufferUsageFlags Flags, VkMemoryPropertyFlags Properties)
	{
		MemorySize = BufferSize;
		LogicalDevice = LD;
		// info to create vertex buffer, not assigning memory
		VkBufferCreateInfo BufferCreateInfo = {};
		BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		BufferCreateInfo.size = BufferSize;
		BufferCreateInfo.usage = Flags;											// Multiple types of buffer possible, vertex buffer here
		BufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;					// only one using in a time, similar to swap chain images

		VkResult Result = vkCreateBuffer(LD, &BufferCreateInfo, nullptr, &Buffer);
		RESULT_CHECK(Result, "Fail to create buffer.");
		if (Result != VK_SUCCESS)
		{
			return false;
		}

		// Get buffer memory requirements
		VkMemoryRequirements MemRequirements;
		vkGetBufferMemoryRequirements(LD, Buffer, &MemRequirements);

		// Allocate memory to buffer
		VkMemoryAllocateInfo MemAllocInfo = {};
		MemAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		MemAllocInfo.allocationSize = MemRequirements.size;
		MemAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(
			PD,
			MemRequirements.memoryTypeBits,				// Index of memory type on Physical Device that has required bit flags
			Properties									// Memory property, is this local_bit or host_bit or others
		);

		// Allocate memory to VKDevieMemory
		Result = vkAllocateMemory(LD, &MemAllocInfo, nullptr, &Memory);
		RESULT_CHECK(Result, "Fail to allocate memory for buffer.");
		if (Result != VK_SUCCESS)
		{
			return false;
		}

		// Bind memory with given vertex buffer, need to free memory
		vkBindBufferMemory(LD, Buffer, Memory, 0);

		return true;
	}

	void cBuffer::cleanUp()
	{
		vkDestroyBuffer(LogicalDevice, Buffer, nullptr);
		vkFreeMemory(LogicalDevice, Memory, nullptr);
	}

}

