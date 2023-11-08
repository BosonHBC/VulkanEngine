#include "Descriptor_Buffer.h"
#include "../Utilities.h"

namespace VKE
{
	bool cDescriptor_Buffer::CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages, FMainDevice* iMainDevice)
	{
		IDescriptor::CreateDescriptor(Type, Binding, Stages, iMainDevice);

		// Ensure that SetDescriptorBufferRange has called such that the buffer has a size
		// And MainDevice is not null
		if (BufferInfo.range <= 0 || ObjectCount <= 0 || !iMainDevice)
		{
			assert(false);
			return false;
		}
		
		return true;
	}

	void cDescriptor_Buffer::SetDescriptorBufferRange(VkDeviceSize BufferFormatSize, uint32_t ObjectCount)
	{
		// For uniform buffer, the size should be the same as the size of desired buffer format
		BufferInfo.range = BufferFormatSize;
		BufferInfo.offset = 0;
		this->ObjectCount = ObjectCount;
	}

	void cDescriptor_Buffer::CreateBuffer(VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags)
	{
		// Create uniform buffer
		if (!Buffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, BufferInfo.range * ObjectCount, UsageFlags, MemoryPropertyFlags))
		{
			return;
		}

		BufferInfo.buffer = Buffer.GetvkBuffer();
	}

	void cDescriptor_Buffer::UpdateBufferData(void* srcData)
	{
		void * pData = nullptr;
		vkMapMemory(Buffer.GetDevice(), Buffer.GetMemory(), 0, BufferInfo.range, 0, &pData);
		memcpy(pData, srcData, static_cast<size_t>(BufferInfo.range));
		vkUnmapMemory(Buffer.GetDevice(), Buffer.GetMemory());
	}

	void cDescriptor_Buffer::UpdatePartialData(void * srcData, VkDeviceSize Offset, VkDeviceSize Size)
	{
		void* pData = nullptr;
		vkMapMemory(Buffer.GetDevice(), Buffer.GetMemory(), Offset, Size, 0, &pData);
		memcpy(pData, srcData, static_cast<size_t>(Size));
		vkUnmapMemory(Buffer.GetDevice(), Buffer.GetMemory());
	}

	void cDescriptor_Buffer::CleanUp()
	{
		Buffer.cleanUp();
	}


	VkWriteDescriptorSet cDescriptor_Buffer::ConstructDescriptorBindingInfo(VkDescriptorSet SetToBind)
	{
		// Data about connection between Descriptor and buffer
		VkWriteDescriptorSet SetWrite = {};

		// Write info
		SetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		SetWrite.dstSet = SetToBind;								// Descriptor set to update
		SetWrite.dstBinding = DescriptorInfo.Binding;					// matches with binding on layout/shader
		SetWrite.dstArrayElement = 0;									// Index in array to update
		SetWrite.descriptorType = DescriptorInfo.Type;					// Type of the descriptor, should match the descriptor set type
		SetWrite.descriptorCount = 1;									// Amount to update
		SetWrite.pBufferInfo = &BufferInfo;								// Information about buffer data to bind
		
		return SetWrite;
	}
}