#include "Descriptor_Dynamic.h"
#include "Utilities.h"
namespace VKE
{

	bool cDescriptor_Dynamic::CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages, /* Properties of the descriptor */
		VkPhysicalDevice PD, VkDevice LD) /* Properties for creating a buffer */
	{
		bool Result = cDescriptor::CreateDescriptor(Type, Binding, Stages, PD, LD);
		if (!Result)
		{
			return Result;
		}
		// allocate memory in CPU
		allocateDynamicBufferTransferSpace();
		return Result;
	}

	void cDescriptor_Dynamic::SetDescriptorBufferRange(VkDeviceSize BufferFormatSize, uint32_t ObjectCount)
	{
		// For dynamic uniform buffer, the size should be aligned with MinUniformBufferOffset
		VkDeviceSize MinAlignment = GetMinUniformOffsetAlignment();
		BufferInfo.range = static_cast<size_t>((BufferFormatSize + MinAlignment) & ~(MinAlignment - 1));
		BufferInfo.offset = 0;
		this->ObjectCount = ObjectCount;
	}

	void cDescriptor_Dynamic::cleanUp()
	{
		cDescriptor::cleanUp();
		// free allocated memory
		_aligned_free(pAllocatedTransferSpace);
	}

	void cDescriptor_Dynamic::allocateDynamicBufferTransferSpace()
	{
		// Create space in memory to hold dynamic buffer that is aligned to our required alignment
		pAllocatedTransferSpace = _aligned_malloc(BufferInfo.range * ObjectCount, BufferInfo.range);
	}

}