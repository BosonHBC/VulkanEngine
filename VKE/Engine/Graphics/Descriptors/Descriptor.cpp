#include "Descriptor.h"
#include "Utilities.h"

namespace VKE
{
	// Data structure to distinguish how many descriptors type the program is using
	// Useful in creating descriptor set pool
	std::set<VkDescriptorType> DescriptorTypeSet;
	const std::set<VkDescriptorType>& cDescriptor::GetDescriptorTypeSet()
	{
		return DescriptorTypeSet;
	}

	bool cDescriptor::CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages,	/* Properties of the descriptor */
		VkPhysicalDevice PD, VkDevice LD)																	/* Properties for creating a buffer */
	{
		// Ensure that SetDescriptorBufferRange has called such that the buffer has a size
		if (BufferSize <= 0)
		{
			assert(false);
			return false;
		}

		// Create buffer
		if (!Buffer.CreateBufferAndAllocateMemory(PD, LD, BufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		{
			return false;
		}

		// Setup info
		DescriptorInfo.Type = Type;
		DescriptorInfo.Binding = Binding;
		DescriptorInfo.Stages = Stages;
		DescriptorTypeSet.insert(DescriptorInfo.Type);
		return true;
	}

	void cDescriptor::SetDescriptorBufferRange(size_t BufferFormatSize, uint32_t ObjectCount)
	{
		// For uniform buffer, the size should be the same as the size of desired buffer format
		BufferSize = BufferFormatSize;
		this->ObjectCount = ObjectCount;
	}

	void cDescriptor::cleanUp()
	{
		Buffer.cleanUp();
	}

	VkWriteDescriptorSet cDescriptor::ConstructDescriptorBindingInfo(VkDescriptorSet DescriptorSet)
	{
		// Data about connection between Descriptor and buffer
		VkWriteDescriptorSet SetWrite = {};

		// Info of the buffer this descriptor is trying connecting with 
		VkDescriptorBufferInfo BufferInfo = {};
		BufferInfo.buffer = Buffer.GetBuffer();					// Buffer to get data from
		BufferInfo.offset = 0;									// offset of the data
		BufferInfo.range = BufferSize;		// Size of data

		// Write info
		SetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		SetWrite.dstSet = DescriptorSet;								// Descriptor set to update
		SetWrite.dstBinding = DescriptorInfo.Binding;					// matches with binding on layout/shader
		SetWrite.dstArrayElement = 0;									// Index in array to update
		SetWrite.descriptorType = DescriptorInfo.Type;					// Type of the descriptor, should match the descriptor set type
		SetWrite.descriptorCount = 1;									// Amount to update
		SetWrite.pBufferInfo = &BufferInfo;								// Information about buffer data to bind
		
		return SetWrite;
	}

}