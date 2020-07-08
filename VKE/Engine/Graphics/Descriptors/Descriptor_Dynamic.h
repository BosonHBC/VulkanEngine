#pragma once
#include "Descriptor.h"

namespace VKE
{
	class cDescriptor_Dynamic : public cDescriptor
	{
	public:
		cDescriptor_Dynamic() : cDescriptor() {}
		~cDescriptor_Dynamic() {};

		/* Create Function */
		// Store descriptor information, create buffer, allocate device memory
		bool CreateDescriptor(
			VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages,		// Properties of the descriptor
			VkPhysicalDevice PD, VkDevice LD										// Properties for creating a buffer
		) override;

		// Calculate the buffer size, different types of buffers should have different size calculations
		void SetDescriptorBufferRange(VkDeviceSize BufferFormatSize, uint32_t ObjectCount) override;

		/** Getters */
		void* GetAllocatedMemory() const { return pAllocatedTransferSpace; }

		/* Clean up Function */
		void cleanUp() override;

	protected:
		// Allocate memory in CPU for copying data to GPU
		void allocateDynamicBufferTransferSpace();

		void * pAllocatedTransferSpace = nullptr;
	};

}