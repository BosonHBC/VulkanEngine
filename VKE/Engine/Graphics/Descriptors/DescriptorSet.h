#pragma once

#include "Utilities.h"
#include <vector>

namespace VKE
{
	enum EDescriptorSetType : uint8_t
	{
		FirstPass_vert,
		ThirdPass_frag,
		FirstPass_frag,
		SecondPass_frag,
		ComputePass,
		Invalid = uint8_t(-1),
	};

	class IDescriptor;
	class cImageBuffer;
	class cDescriptorSet
	{
	public:
		/** Static functions */
		static void CleanupDescriptorSetLayout(FMainDevice* iMainDevice);
		static VkDescriptorSetLayout GetDescriptorSetLayout(EDescriptorSetType iType);
	public:
		/** Constructors */
		cDescriptorSet() {};
		cDescriptorSet(FMainDevice* iMainDevice) : pMainDevice(iMainDevice) {}
		~cDescriptorSet() {};
		cDescriptorSet(const cDescriptorSet& i_other) = default;
		cDescriptorSet& operator = (const cDescriptorSet& i_other) = delete;
		cDescriptorSet( cDescriptorSet&& i_other) = default;
		cDescriptorSet& operator = (cDescriptorSet&& i_other) = default;
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
		
		template <class T>
		const T* const GetDescriptorAt_Immutable(size_t Idx) const
		{
			if (Idx >= Descriptors.size())
			{
				return nullptr;
			}

			T* Descriptor = dynamic_cast<T*>(Descriptors[Idx]);
			return Descriptor;
		}
		const VkDescriptorSetLayout& GetDescriptorSetLayout() const;
		const VkDescriptorSet& GetDescriptorSet() const { return DescriptorSet; }

		// Create different types of descriptors
		void CreateBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage);

		void CreateDynamicBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage);

		void CreateImageBufferDescriptor(cImageBuffer* const & iImageBuffer, VkDescriptorType Type, VkShaderStageFlags ShaderStage, VkImageLayout ImageLayout, VkSampler Sampler = VK_NULL_HANDLE);

		void CreateStorageBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags);

		// Create Descriptor set layout
		void CreateDescriptorSetLayout(EDescriptorSetType iDescriptorType);
		
		// Allocate Descriptor Set
		void AllocateDescriptorSet(VkDescriptorPool Pool);
		
		// Bind Descriptor's content to the descriptor set
		void BindDescriptorWithSet();

		FMainDevice* pMainDevice = nullptr;
	protected:

		// Used for querying the descriptor set layout
		EDescriptorSetType DescriptorSetType = Invalid;
		// Descriptors that hold data
		std::vector<IDescriptor*> Descriptors;
		// Descriptor Set that has the layout determined by DescriptorSetLayout and hold store descriptors
		VkDescriptorSet DescriptorSet;
	};
}