#include "Descriptor.h"
#include "../Utilities.h"

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
		if (BufferInfo.range <= 0 || ObjectCount <= 0)
		{
			assert(false);
			return false;
		}

		// Create uniform buffer
		if (!Buffer.CreateBufferAndAllocateMemory(PD, LD, BufferInfo.range * ObjectCount,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		{
			return false;
		}

		// Setup info
		DescriptorInfo.Type = Type;
		DescriptorInfo.Binding = Binding;
		DescriptorInfo.Stages = Stages;
		BufferInfo.buffer = Buffer.GetBuffer();
		DescriptorTypeSet.insert(DescriptorInfo.Type);
		return true;
	}

	void cDescriptor::SetDescriptorBufferRange(VkDeviceSize BufferFormatSize, uint32_t ObjectCount)
	{
		// For uniform buffer, the size should be the same as the size of desired buffer format
		BufferInfo.range = BufferFormatSize;
		BufferInfo.offset = 0;
		this->ObjectCount = ObjectCount;
	}

	void cDescriptor::UpdateBufferData(void* srcData)
	{
		void * pData = nullptr;
		vkMapMemory(Buffer.GetDevice(), Buffer.GetMemory(), 0, BufferInfo.range, 0, &pData);
		memcpy(pData, srcData, static_cast<size_t>(BufferInfo.range));
		vkUnmapMemory(Buffer.GetDevice(), Buffer.GetMemory());
	}

	void cDescriptor::UpdatePartialData(void * srcData, VkDeviceSize Offset, VkDeviceSize Size)
	{
		void* pData = nullptr;
		vkMapMemory(Buffer.GetDevice(), Buffer.GetMemory(), Offset, Size, 0, &pData);
		memcpy(pData, srcData, static_cast<size_t>(Size));
		vkUnmapMemory(Buffer.GetDevice(), Buffer.GetMemory());
	}

	void cDescriptor::cleanUp()
	{
		Buffer.cleanUp();
	}

	VkWriteDescriptorSet cDescriptor::ConstructDescriptorBindingInfo(VkDescriptorSet DescriptorSet)
	{
		// Data about connection between Descriptor and buffer
		VkWriteDescriptorSet SetWrite = {};

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

	VkDescriptorSetLayoutBinding cDescriptor::ConstructDescriptorSetLayoutBinding()
	{
		VkDescriptorSetLayoutBinding Binding = {};
		// FrameData binding info
		Binding.binding = DescriptorInfo.Binding;				// binding in the uniform struct in shader
		Binding.descriptorType = DescriptorInfo.Type;			// Type of descriptor, Uniform, dynamic_uniform, image_sampler ...
		Binding.descriptorCount = 1;							// Number of descriptor for binding
		Binding.stageFlags = DescriptorInfo.Stages;				// Shader stage to bind to
		Binding.pImmutableSamplers = nullptr;					// for textures setting, if texture is immutable or not
		return Binding;
	}

}