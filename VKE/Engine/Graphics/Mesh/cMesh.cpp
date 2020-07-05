#include "cMesh.h"

namespace VKE
{
	cMesh::cMesh(const FMainDevice& iMainDevice, const std::vector<FVertex>& iVertices)
	{
		VertexCount = iVertices.size();
		MainDevice = iMainDevice;
		createVertexBuffer(iVertices, VertexBuffer);
	}

	void cMesh::cleanUpVertexBuffer()
	{
		vkDestroyBuffer(MainDevice.LD, VertexBuffer, nullptr);
		vkFreeMemory(MainDevice.LD, VertexBufferMemory, nullptr);
	}

	bool cMesh::createVertexBuffer(const std::vector<FVertex>& iVertices, VkBuffer& oBuffer)
	{
		// info to create vertex buffer, not assigning memory
		VkBufferCreateInfo VertexBufferCreateInfo = {};
		VertexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		VertexBufferCreateInfo.size = sizeof(FVertex) * iVertices.size();
		VertexBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;				// Multiple types of buffer possible, vertex buffer here
		VertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;					// only one using in a time, similar to swap chain images

		VkResult Result = vkCreateBuffer(MainDevice.LD, &VertexBufferCreateInfo, nullptr, &oBuffer);
		RESULT_CHECK(Result, "Fail to create vertex buffer.");
		if(Result != VK_SUCCESS)
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
		MemAllocInfo.memoryTypeIndex = findMemoryTypeIndex(
			MemRequirements.memoryTypeBits,				// Index of memory type on Physical Device that has required bit flags
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |		// CPU can interact with the memory
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT		// Allows placement of data straight into buffer after mapping (otherwise would have to specify manually), no need to flush the data
		);	

		// Allocate memory to VKDevieMemory
		Result = vkAllocateMemory(MainDevice.LD, &MemAllocInfo, nullptr, &VertexBufferMemory);
		RESULT_CHECK(Result, "Fail to allocate vertex buffer.");
		if (Result != VK_SUCCESS)
		{
			return false;
		}

		// Bind memory with given vertex buffer, need to free memory
		vkBindBufferMemory(MainDevice.LD, oBuffer, VertexBufferMemory, 0);

		// Map memory to vertex buffer
		void * VertexData = nullptr;																	// 1. Create pointer to a point in random memory;
		vkMapMemory(MainDevice.LD, VertexBufferMemory, 0, VertexBufferCreateInfo.size, 0, &VertexData);	// 2. Map the vertex buffer memory to that point
		memcpy(VertexData, iVertices.data(), static_cast<size_t>(VertexBufferCreateInfo.size));			// 3. copy the data
		vkUnmapMemory(MainDevice.LD, VertexBufferMemory);												// 4. unmap the vertex buffer memory, if not using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, need to flush
		
		return true;
	}

	uint32_t cMesh::findMemoryTypeIndex(uint32_t AllowedTypes, VkMemoryPropertyFlags Properties)
	{
		// Get properties of physical device memory
		VkPhysicalDeviceMemoryProperties MemProperties;
		vkGetPhysicalDeviceMemoryProperties(MainDevice.PD, &MemProperties);

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

}