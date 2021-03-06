/*
	Buffer is a general buffer that can storage data in GPU
	Can be used for staging, vertex, index
*/
#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace VKE
{
	class cBuffer
	{
	public:
		cBuffer() {};
		~cBuffer() {};

		// Create buffer and allocate memory for any specific usage type of buffer
		bool CreateBufferAndAllocateMemory(VkPhysicalDevice PD, VkDevice LD, VkDeviceSize BufferSize, VkBufferUsageFlags Flags, VkMemoryPropertyFlags Properties);
		void cleanUp();

		const VkBuffer& GetvkBuffer() const { return Buffer; }
		const VkBuffer& GetMemory() const { return Memory; }
		const VkDevice& GetDevice() const { return LogicalDevice; }
		VkDeviceSize BufferSize() const { return MemorySize; }

	protected:
		VkDevice LogicalDevice;
		VkBuffer Buffer;
		VkDeviceMemory Memory;
		VkDeviceSize MemorySize;
	};


}