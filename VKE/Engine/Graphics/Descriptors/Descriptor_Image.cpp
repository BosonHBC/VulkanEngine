#include "Descriptor_Image.h"

namespace VKE
{

	bool cDescriptor_Image::CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages, FMainDevice* iMainDevice)
	{
		return IDescriptor::CreateDescriptor(Type, Binding, Stages, iMainDevice);
	}

	VkWriteDescriptorSet cDescriptor_Image::ConstructDescriptorBindingInfo(VkDescriptorSet SetToBind)
	{
		VkWriteDescriptorSet SetWrite = {};
		SetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		SetWrite.dstSet = SetToBind;
		SetWrite.dstBinding = DescriptorInfo.Binding;
		SetWrite.dstArrayElement = 0;
		SetWrite.descriptorType = DescriptorInfo.Type;
		SetWrite.descriptorCount = 1;
		SetWrite.pImageInfo = &ImageInfo;

		return SetWrite;
	}

	void cDescriptor_Image::SetImageBuffer(cImageBuffer* const & iImageBuffer, VkImageLayout ImageLayout, VkSampler Sampler /*= VK_NULL_HANDLE*/)
	{
		if (!iImageBuffer)
		{
			return;
		}
		pImageBuffer = iImageBuffer;
		ImageInfo.imageLayout = ImageLayout;					// Layout in GPU to be used
		ImageInfo.imageView = pImageBuffer->GetImageView();
		ImageInfo.sampler = Sampler;
	}

	void cDescriptor_Image::cleanUp()
	{
		
	}

}