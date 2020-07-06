#include "cMesh.h"

namespace VKE
{
	cMesh::cMesh(const FMainDevice& iMainDevice,
		VkQueue TransferQueue, VkCommandPool TransferCommandPool,
		const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices)
	{
		VertexBuffer.Count = iVertices.size();
		IndexBuffer.Count = iIndices.size();
		MainDevice = iMainDevice;
		createVertexBuffer(iVertices, TransferQueue, TransferCommandPool);
		createIndexBuffer(iIndices, TransferQueue, TransferCommandPool);
		DrawCallData.ModelMatrix = glm::mat4(1.0f);
	}

	void cMesh::cleanUp()
	{
		vkDestroyBuffer(MainDevice.LD, VertexBuffer.Buffer, nullptr);
		vkFreeMemory(MainDevice.LD, VertexBuffer.DeviceMemory, nullptr);

		vkDestroyBuffer(MainDevice.LD, IndexBuffer.Buffer, nullptr);
		vkFreeMemory(MainDevice.LD, IndexBuffer.DeviceMemory, nullptr);
	}

	bool cMesh::createVertexBuffer(const std::vector<FVertex>& iVertices, VkQueue TransferQueue, VkCommandPool TransferCommandPool)
	{
		VkDeviceSize BufferSize = sizeof(FVertex) * iVertices.size();

		// Create temporary buffer to "stage" data before transferring to GPU
		VkBuffer StagingBuffer;
		VkDeviceMemory StagingBufferMemory;

		if (!CreateBufferAndAllocateMemory(MainDevice, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// Transfer source buffer
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |		// CPU can interact with the memory
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,		// Allows placement of data straight into buffer after mapping (otherwise would have to specify manually), no need to flush the data
			StagingBuffer,
			StagingBufferMemory
		)) return false;

		// Map memory to the staging buffer
		void * VertexData = nullptr;																	// 1. Create pointer to a point in random memory;
		vkMapMemory(MainDevice.LD, StagingBufferMemory, 0, BufferSize, 0, &VertexData);					// 2. Map the vertex buffer memory to that point
		memcpy(VertexData, iVertices.data(), static_cast<size_t>(BufferSize));							// 3. copy the data
		vkUnmapMemory(MainDevice.LD, StagingBufferMemory);												// 4. unmap the vertex buffer memory, if not using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, need to flush

		// Create buffer with TRANSFER_DST_BIT to mark as recipient of transfer data, it is also a vertex buffer
		if (!CreateBufferAndAllocateMemory(MainDevice, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |			// Transfer destination buffer
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// Also a vertex buffer
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,		// Only local visible to GPU, not visible on CPU
			VertexBuffer.Buffer,
			VertexBuffer.DeviceMemory
		)) return false;

		// Copy staging buffer to vertex buffer in GPU
		CopyBuffer(MainDevice.LD, TransferQueue, TransferCommandPool, StagingBuffer, VertexBuffer.Buffer, BufferSize);

		// Clean up staging buffer parts
		vkDestroyBuffer(MainDevice.LD, StagingBuffer, nullptr);
		vkFreeMemory(MainDevice.LD, StagingBufferMemory, nullptr);
		return true;
	}


	bool cMesh::createIndexBuffer(const std::vector<uint32_t>& iIndices, VkQueue TransferQueue, VkCommandPool TransferCommandPool)
	{
		VkDeviceSize BufferSize = sizeof(uint32_t) * iIndices.size();

		// Create temporary buffer to "stage" data before transferring to GPU
		VkBuffer StagingBuffer;
		VkDeviceMemory StagingBufferMemory;

		if (!CreateBufferAndAllocateMemory(MainDevice, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// Transfer source buffer
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |		// CPU can interact with the memory
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,		// Allows placement of data straight into buffer after mapping (otherwise would have to specify manually), no need to flush the data
			StagingBuffer,
			StagingBufferMemory
		)) return false;

		// Map memory to the staging buffer
		void * IndexData = nullptr;																		// 1. Create pointer to a point in random memory;
		vkMapMemory(MainDevice.LD, StagingBufferMemory, 0, BufferSize, 0, &IndexData);					// 2. Map the index buffer memory to that point
		memcpy(IndexData, iIndices.data(), static_cast<size_t>(BufferSize));							// 3. copy the data
		vkUnmapMemory(MainDevice.LD, StagingBufferMemory);												// 4. unmap the index buffer memory, if not using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, need to flush

		// Create buffer with TRANSFER_DST_BIT to mark as recipient of transfer data, it is also a index buffer
		if (!CreateBufferAndAllocateMemory(MainDevice, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |			// Transfer destination buffer
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,			// Also a index buffer
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,		// Only local visible to GPU, not visible on CPU
			IndexBuffer.Buffer,
			IndexBuffer.DeviceMemory
		)) return false;

		// Copy staging buffer to index buffer in GPU
		CopyBuffer(MainDevice.LD, TransferQueue, TransferCommandPool, StagingBuffer, IndexBuffer.Buffer, BufferSize);

		// Clean up staging buffer parts
		vkDestroyBuffer(MainDevice.LD, StagingBuffer, nullptr);
		vkFreeMemory(MainDevice.LD, StagingBufferMemory, nullptr);
		return true;
	}

}