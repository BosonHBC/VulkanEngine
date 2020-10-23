#include "DescriptorSet.h"
#include "Descriptor_Buffer.h"
#include "Descriptor_Dynamic.h"
#include "Descriptor_Image.h"

#include <map>
namespace VKE
{
	std::map<EDescriptorSetType, VkDescriptorSetLayout> SDescriptorSetLayoutMap;


	void cDescriptorSet::CreateBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage)
	{
		cDescriptor_Buffer* newBufferDescriptor = DBG_NEW cDescriptor_Buffer();
		newBufferDescriptor->SetDescriptorBufferRange(BufferFormatSize, ObjectCount);
		newBufferDescriptor->CreateDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Descriptors.size(), ShaderStage, pMainDevice);
		newBufferDescriptor->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		Descriptors.push_back(newBufferDescriptor);
	}

	void cDescriptorSet::CreateDynamicBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage)
	{
		cDescriptor_DynamicBuffer* newDBufferDescriptor = DBG_NEW cDescriptor_DynamicBuffer();
		newDBufferDescriptor->SetDescriptorBufferRange(BufferFormatSize, ObjectCount);
		newDBufferDescriptor->CreateDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Descriptors.size(), ShaderStage, pMainDevice);
		newDBufferDescriptor->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		Descriptors.push_back(newDBufferDescriptor);
	}

	void cDescriptorSet::CreateImageBufferDescriptor(cImageBuffer* const & iImageBuffer, VkDescriptorType Type, VkShaderStageFlags ShaderStage, VkImageLayout ImageLayout, VkSampler Sampler /*= VK_NULL_HANDLE*/)
	{
		cDescriptor_Image* newImageDescriptor = DBG_NEW cDescriptor_Image();
		newImageDescriptor->CreateDescriptor(Type, Descriptors.size(), ShaderStage, pMainDevice);
		newImageDescriptor->SetImageBuffer(iImageBuffer, ImageLayout, Sampler);

		Descriptors.push_back(newImageDescriptor);
	}

	void cDescriptorSet::CreateStorageBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage, VkBufferUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryPropertyFlags)
	{
		cDescriptor_Buffer* newSBufferDescriptor = DBG_NEW cDescriptor_Buffer();
		newSBufferDescriptor->SetDescriptorBufferRange(BufferFormatSize, ObjectCount);
		newSBufferDescriptor->CreateDescriptor(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Descriptors.size(), ShaderStage, pMainDevice);
		newSBufferDescriptor->CreateBuffer(UsageFlags, MemoryPropertyFlags);

		Descriptors.push_back(newSBufferDescriptor);
	}

	void cDescriptorSet::CreateDescriptorSetLayout(EDescriptorSetType iDescriptorType)
	{
		DescriptorSetType = iDescriptorType;
		// if this type of descriptor set layout has not been created, create one.
		if (SDescriptorSetLayoutMap.find(DescriptorSetType) == SDescriptorSetLayoutMap.end())
		{
			std::vector<VkDescriptorSetLayoutBinding> Bindings(Descriptors.size(), VkDescriptorSetLayoutBinding());

			for (size_t i = 0; i < Bindings.size(); ++i)
			{
				Bindings[i] = Descriptors[i]->ConstructDescriptorSetLayoutBinding();
			}

			VkDescriptorSetLayoutCreateInfo LayoutCreateInfo;
			LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			LayoutCreateInfo.bindingCount = Bindings.size();
			LayoutCreateInfo.pBindings = Bindings.data();
			LayoutCreateInfo.pNext = nullptr;
			LayoutCreateInfo.flags = 0;

			VkDescriptorSetLayout Layout;
			VkResult Result = vkCreateDescriptorSetLayout(pMainDevice->LD, &LayoutCreateInfo, nullptr, &Layout);
			RESULT_CHECK(Result, "Fail to create descriptor set layout.");

			SDescriptorSetLayoutMap.insert({ DescriptorSetType, Layout });
		}
		
	}

	void cDescriptorSet::AllocateDescriptorSet(VkDescriptorPool Pool)
	{
		VkDescriptorSetAllocateInfo SetAllocInfo = {};
		SetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		SetAllocInfo.descriptorPool = Pool;													// Pool to allocate descriptor set from
		SetAllocInfo.descriptorSetCount = 1;									// Number of sets to allocate
		SetAllocInfo.pSetLayouts = &GetDescriptorSetLayout();

		// Allocate descriptor sets (multiple)
		VkResult Result = vkAllocateDescriptorSets(pMainDevice->LD, &SetAllocInfo, &DescriptorSet);
		RESULT_CHECK(Result, "Fail to Allocate Descriptor Set!");
	}

	void cDescriptorSet::BindDescriptorWithSet()
	{
		// Data about connection between Descriptor and buffer
		std::vector<VkWriteDescriptorSet> SetWrites(Descriptors.size(), VkWriteDescriptorSet());
		for (size_t i = 0; i < SetWrites.size(); ++i)
		{
			SetWrites[i] = Descriptors[i]->ConstructDescriptorBindingInfo(DescriptorSet);
		}
		// Update the descriptor sets with new buffer / binding info
		vkUpdateDescriptorSets(pMainDevice->LD, static_cast<uint32_t>(SetWrites.size()), SetWrites.data(),
			0, nullptr // Allows copy descriptor set to another descriptor set
		);
	}

	void cDescriptorSet::CleanupDescriptorSetLayout(FMainDevice* iMainDevice)
	{
		for (auto Value : SDescriptorSetLayoutMap)
		{
			vkDestroyDescriptorSetLayout(iMainDevice->LD, Value.second, nullptr);
		}
		SDescriptorSetLayoutMap.clear();
	}

	void cDescriptorSet::cleanUp()
	{
		for (auto Descriptor : Descriptors)
		{
			Descriptor->cleanUp();
			safe_delete(Descriptor);
		}
		Descriptors.clear();
		pMainDevice = nullptr;
	}

	const VkDescriptorSetLayout& cDescriptorSet::GetDescriptorSetLayout() const
	{
		return SDescriptorSetLayoutMap.at(DescriptorSetType);
	}

	VkDescriptorSetLayout cDescriptorSet::GetDescriptorSetLayout(EDescriptorSetType iType)
	{
		if (SDescriptorSetLayoutMap.find(iType) == SDescriptorSetLayoutMap.end())
		{
			// can not find this set layout, return 0
			return SDescriptorSetLayoutMap.at(EDescriptorSetType(0));
		}
		return SDescriptorSetLayoutMap.at(iType);
	}

}