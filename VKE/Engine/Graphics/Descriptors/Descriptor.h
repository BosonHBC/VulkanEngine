#pragma once
#include "Utilities.h"

namespace VKE
{
	struct FDescriptorInfo
	{
		VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		uint32_t Binding = 0;
		VkShaderStageFlags Stages = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	};
	/*
		Interface of All kinds of descriptors: Buffer, Image
	*/
	struct FMainDevice;
	class IDescriptor
	{

	public:
		virtual ~IDescriptor() {}
		/** Getters */
		const FDescriptorInfo& GetDescriptorInfo() const { return DescriptorInfo; }

		/** Construction functions */
		// 1. Store descriptor information. In child class, this function may generate contents for the descriptor also
		virtual bool CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages, FMainDevice* iMainDevice)
		{
			pMainDevice = iMainDevice;
			DescriptorInfo.Type = Type;
			DescriptorInfo.Binding = Binding;
			DescriptorInfo.Stages = Stages;
			return true;
		}

		// Helper function to get information when creating descriptor set layout
		virtual VkDescriptorSetLayoutBinding ConstructDescriptorSetLayoutBinding()
		{
			VkDescriptorSetLayoutBinding Binding = {};

			Binding.binding = DescriptorInfo.Binding;				// binding in the uniform struct in shader
			Binding.descriptorType = DescriptorInfo.Type;			// Type of descriptor, Uniform, dynamic_uniform, image_sampler ...
			Binding.descriptorCount = 1;							// Number of descriptor for binding
			Binding.stageFlags = DescriptorInfo.Stages;				// Shader stage to bind to
			Binding.pImmutableSamplers = nullptr;					// for textures setting, if texture is immutable or not
			return Binding;
		}
		// Helper function to get information when binding content to the descriptor set
		virtual VkWriteDescriptorSet ConstructDescriptorBindingInfo(VkDescriptorSet SetToBind) = 0;
	
		virtual void CleanUp() = 0;
	protected:
		// Information of this descriptor
		FDescriptorInfo DescriptorInfo;
		FMainDevice* pMainDevice = nullptr;
	};
}
