#pragma once
#include "Descriptor.h"
#include "Buffer/ImageBuffer.h"
namespace VKE
{
	class cDescriptor_Image : public IDescriptor
	{
	public:

		// ===== Start of IDescriptor ======
		/** Construction functions */
		virtual bool CreateDescriptor(VkDescriptorType Type, uint32_t Binding, VkShaderStageFlags Stages, FMainDevice* iMainDevice) override;

		virtual VkWriteDescriptorSet ConstructDescriptorBindingInfo(VkDescriptorSet SetToBind) override;

		// ===== End of IDescriptor ======

		/** Constructors */
		cDescriptor_Image() {};
		virtual ~cDescriptor_Image() {};

		void SetImageBuffer(cImageBuffer* const & iImageBuffer, VkImageLayout ImageLayout, VkSampler Sampler = VK_NULL_HANDLE);

		/* Clean up Function */
		virtual void cleanUp();

		/** Getters */

	protected:
		// Reference to the Image Buffer
		cImageBuffer* pImageBuffer = nullptr;

		// Info of the Image buffer this descriptor is trying connecting with 
		VkDescriptorImageInfo ImageInfo;
	};
}