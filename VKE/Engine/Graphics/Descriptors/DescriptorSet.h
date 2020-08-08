#pragma once

#include "Utilities.h"
#include <vector>

namespace VKE
{
	class IDescriptor;
	class cImageBuffer;
	class cDescriptorSet
	{
	public:
		/** Constructors */
		cDescriptorSet() = delete;
		cDescriptorSet(FMainDevice* iMainDevice) : pMainDevice(iMainDevice) {}
		~cDescriptorSet() {};
		cDescriptorSet(const cDescriptorSet& i_other) = default;
		cDescriptorSet& operator = (const cDescriptorSet& i_other) = delete;
		cDescriptorSet( cDescriptorSet&& i_other) = default;
		cDescriptorSet& operator = (cDescriptorSet&& i_other) = delete;
		void cleanUp();

		/** Getters */
		template <class T>
		T* GetDescriptorAt(size_t Idx)
		{
			if (Idx >= Descriptors.size())
			{
				return nullptr;
			}

			T* Descriptor = dynamic_cast<T*>(Descriptors[Idx]);
			return Descriptor;
		}

		const VkDescriptorSetLayout& GetDescriptorSetLayout() const { return DescriptorSetLayout; }
		const VkDescriptorSet& GetDescriptorSet() const { return DescriptorSet; }

		// Create different types of descriptors
		void CreateBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage);

		void CreateDynamicBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage);

		void CreateImageBufferDescriptor(cImageBuffer* const & iImageBuffer, VkDescriptorType Type, VkShaderStageFlags ShaderStage, VkImageLayout ImageLayout, VkSampler Sampler = VK_NULL_HANDLE);

		// Create Descriptor set layout
		void CreateDescriptorSetLayout();
		
		// Allocate Descriptor Set
		void AllocateDescriptorSet(VkDescriptorPool Pool);
		
		// Bind Descriptor's content to the descriptor set
		void BindDescriptorWithSet();

		
	protected:
		FMainDevice* pMainDevice = nullptr;
		// Descriptor set layout
		VkDescriptorSetLayout DescriptorSetLayout;
		// Descriptors that hold data
		std::vector<IDescriptor*> Descriptors;
		// Descriptor Set that has the layout determined by DescriptorSetLayout and hold store descriptors
		VkDescriptorSet DescriptorSet;
	};
}