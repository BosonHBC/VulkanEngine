#pragma once
#include <set>

#include "Buffer/Buffer.h"
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
		/** Constructors */
		cDescriptor() {};
		virtual ~cDescriptor() {};

		/* Create Function */
		// Store descriptor information, create buffer, allocate device memory
		virtual bool CreateDescriptor( 
			VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages,	// Properties of the descriptor
			VkPhysicalDevice PD, VkDevice LD									// Properties for creating a buffer
			
		);
		
		// Calculate the buffer size, different types of buffers should have different size calculations
		virtual void SetDescriptorBufferRange(VkDeviceSize BufferFormatSize, uint32_t ObjectCount);
		
		/* Update Function */
		// Update the full memory block
		void UpdateBufferData(void* srcData);
		void UpdatePartialData(void * srcData, VkDeviceSize Offset, VkDeviceSize Size);

		/* Clean up Function */
		virtual void cleanUp();

		/** Getters */
		const FDescriptorInfo& GetDescriptorInfo() const { return DescriptorInfo; }
		const VkDeviceMemory& GetBufferMemory() const { return Buffer.GetMemory(); }
		const VkDeviceSize& GetSlotSize() const { return BufferInfo.range; }
		VkDescriptorSet* GetDescriptorSet() const { return pDescriptorSet; }
		
		// Allocate descriptor set for this descriptor
		void SetDescriptorSet(VkDescriptorSet* iDescriptorSet) { pDescriptorSet = iDescriptorSet; }
		// Helper function to get information when creating descriptor set
		VkWriteDescriptorSet ConstructDescriptorBindingInfo();
		
		// Helper function to get information when creating descriptor set layout
		virtual VkDescriptorSetLayoutBinding ConstructDescriptorSetLayoutBinding();
	
	protected:
		// Holding actual data of descriptor in GPU
		cBuffer Buffer;
		
		// Information of this descriptor
		FDescriptorInfo DescriptorInfo;
		
		// Descriptor Set this descriptor is in
		VkDescriptorSet* pDescriptorSet;

		// Info of the buffer this descriptor is trying connecting with 
		// Buffer size: considering alignment, also used for allocating memory in both CPU and GPu
		VkDescriptorBufferInfo BufferInfo;

		// Object count should be considered in allocating memory
		uint32_t ObjectCount;

	};

}