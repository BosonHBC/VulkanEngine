#pragma once

#include "Buffer/Buffer.h"
#include <set>
namespace VKE
{
	struct FDescriptorInfo
	{
		VkDescriptorType Type;
		uint32_t Binding;
		VkShaderStageFlags Stages;
	};
	// This class contains information of a descriptor which helps descriptor set creation
	// Also contains the buffer binds to the descriptor set
	class cDescriptor
	{
	public:
		/** Static functions */
		static const std::set<VkDescriptorType>& GetDescriptorTypeSet();
		/** Constructors */
		cDescriptor() {};
		~cDescriptor() {};

		/* Create Function */
		// Store descriptor information, create buffer, allocate device memory
		virtual bool CreateDescriptor(
			VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages,		// Properties of the descriptor
			VkPhysicalDevice PD, VkDevice LD										// Properties for creating a buffer
		);
		// Calculate the buffer size, different types of buffers should have different size calculations
		virtual void SetDescriptorBufferRange(size_t BufferFormatSize, uint32_t ObjectCount);

		/* Clean up Function */
		virtual void cleanUp();

		/** Getters */
		const FDescriptorInfo& GetDescriptorInfo() const { return DescriptorInfo; }

		// Helper function when creating descriptor set
		VkWriteDescriptorSet ConstructDescriptorBindingInfo(VkDescriptorSet DescriptorSet);

	protected:
		// Holding actual data of descriptor in GPU
		cBuffer Buffer;
		FDescriptorInfo DescriptorInfo;

		// Buffer size considering alignment, also used for allocating memory in both CPU and GPu
		VkDeviceSize BufferSize;
		// Object count should be considered in allocating memory
		uint32_t ObjectCount;

	};

}