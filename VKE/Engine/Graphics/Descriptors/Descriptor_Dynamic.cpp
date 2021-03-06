#include "Descriptor_Dynamic.h"
#include "Utilities.h"
namespace VKE
{
	bool cDescriptor_DynamicBuffer::CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages, FMainDevice* iMainDevice)
	{
		bool Result = cDescriptor_Buffer::CreateDescriptor(Type, Binding, Stages, iMainDevice);
		if (!Result)
		{
			return Result;
		}
		// allocate memory in CPU
		allocateDynamicBufferTransferSpace();
		return Result;
	}

	void cDescriptor_DynamicBuffer::SetDescriptorBufferRange(VkDeviceSize BufferFormatSize, uint32_t ObjectCount)
	{
		// For dynamic uniform buffer, the size should be aligned with MinUniformBufferOffset
		VkDeviceSize MinAlignment = GetMinUniformOffsetAlignment();
		BufferInfo.range = static_cast<size_t>((BufferFormatSize + MinAlignment) & ~(MinAlignment - 1));
		BufferInfo.offset = 0;
		this->ObjectCount = ObjectCount;
	}

	void cDescriptor_DynamicBuffer::cleanUp()
	{
		cDescriptor_Buffer::cleanUp();
		// free allocated memory
		_aligned_free(pAllocatedTransferSpace);
	}

	void cDescriptor_DynamicBuffer::allocateDynamicBufferTransferSpace()
	{
		// Create space in memory to hold dynamic buffer that is aligned to our required alignment
		pAllocatedTransferSpace = _aligned_malloc(static_cast<size_t>(BufferInfo.range * ObjectCount), static_cast<size_t>(BufferInfo.range));
	}

}