#include "DescriptorSet.h"
#include "Descriptor_Buffer.h"
#include "Descriptor_Dynamic.h"
#include "Descriptor_Image.h"
namespace VKE
{

	void cDescriptorSet::CreateBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage)
	{
		cDescriptor_Buffer* newBufferDescriptor = DBG_NEW cDescriptor_Buffer();
		newBufferDescriptor->SetDescriptorBufferRange(BufferFormatSize, ObjectCount);
		newBufferDescriptor->CreateDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Descriptors.size(), ShaderStage, pMainDevice);

		Descriptors.push_back(newBufferDescriptor);
	}

	void cDescriptorSet::CreateDynamicBufferDescriptor(VkDeviceSize BufferFormatSize, uint32_t ObjectCount, VkShaderStageFlags ShaderStage)
	{
		cDescriptor_DynamicBuffer* newDBufferDescriptor = DBG_NEW cDescriptor_DynamicBuffer();
		newDBufferDescriptor->SetDescriptorBufferRange(BufferFormatSize, ObjectCount);
		newDBufferDescriptor->CreateDescriptor(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, Descriptors.size(), ShaderStage, pMainDevice);

		Descriptors.push_back(newDBufferDescriptor);
	}

	void cDescriptorSet::CreateImageBufferDescriptor(cImageBuffer* const & iImageBuffer, VkDescriptorType Type, VkShaderStageFlags ShaderStage, VkImageLayout ImageLayout, VkSampler Sampler /*= VK_NULL_HANDLE*/)
	{
		cDescriptor_Image* newImageDescriptor = DBG_NEW cDescriptor_Image();
		newImageDescriptor->CreateDescriptor(Type, Descriptors.size(), ShaderStage, pMainDevice);
		newImageDescriptor->SetImageBuffer(iImageBuffer, ImageLayout, Sampler);

		Descriptors.push_back(newImageDescriptor);
	}

	void cDescriptorSet::CreateDescriptorSetLayout()
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

		VkResult Result = vkCreateDescriptorSetLayout(pMainDevice->LD, &LayoutCreateInfo, nullptr, &DescriptorSetLayout);
		RESULT_CHECK(Result, "Fail to create descriptor set layout.")
	}

	void cDescriptorSet::AllocateDescriptorSet(VkDescriptorPool Pool)
	{
		VkDescriptorSetAllocateInfo SetAllocInfo = {};
		SetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		SetAllocInfo.descriptorPool = Pool;													// Pool to allocate descriptor set from
		SetAllocInfo.descriptorSetCount = 1;									// Number of sets to allocate
		SetAllocInfo.pSetLayouts = &DescriptorSetLayout;

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

	void cDescriptorSet::cleanUp()
	{
		vkDestroyDescriptorSetLayout(pMainDevice->LD, DescriptorSetLayout, nullptr);

		for (auto Descriptor : Descriptors)
		{
			Descriptor->cleanUp();
			safe_delete(Descriptor);
		}
		Descriptors.clear();
		pMainDevice = nullptr;
	}

}