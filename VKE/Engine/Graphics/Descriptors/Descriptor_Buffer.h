#pragma once
#include <set>
#include "Descriptor.h"
#include "Buffer/Buffer.h"
namespace VKE
{
	// Buffer descriptor such as uniform buffer, storage buffer
	class cDescriptor_Buffer : public IDescriptor
	{
	public:

		// ===== Start of IDescriptor ======
		/** Construction functions */
		virtual bool CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages, FMainDevice* iMainDevice) override;

		virtual VkWriteDescriptorSet ConstructDescriptorBindingInfo(VkDescriptorSet SetToBind) override;

		// ===== End of IDescriptor ======
		
		/** Constructors */
		cDescriptor_Buffer() {};
		virtual ~cDescriptor_Buffer() {};

		// Calculate the buffer size, different types of buffers should have different size calculations
		virtual void SetDescriptorBufferRange(VkDeviceSize BufferFormatSize, uint32_t ObjectCount);
		virtual void CreateBuffer(VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags);
		/* Update Function */
		// Update the full memory block
		void UpdateBufferData(void* srcData);
		void UpdatePartialData(void * srcData, VkDeviceSize Offset, VkDeviceSize Size);

		/* Clean up Function */
		virtual void cleanUp();

		/** Getters */
		// Get buffer size for Descriptor_buffer
		const cBuffer& GetBuffer() const { return Buffer; }
		const VkDeviceSize& GetSlotSize() const { return BufferInfo.range; }
		const VkDeviceMemory& GetBufferMemory() const { return Buffer.GetMemory(); }



	protected:
		// Holding actual data of descriptor in GPU
		cBuffer Buffer;

		// Info of the buffer this descriptor is trying connecting with 
		// Buffer size: considering alignment, also used for allocating memory in both CPU and GPu
		VkDescriptorBufferInfo BufferInfo;

		// Object count should be considered in allocating memory
		uint32_t ObjectCount;

	};

}