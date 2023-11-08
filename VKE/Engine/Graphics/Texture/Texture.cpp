#include "Texture.h"
#include "Buffer/Buffer.h"

#include <map>

namespace VKE
{
	std::map<std::string, std::shared_ptr<VKE::FTexture>> s_TextureContainer;
	uint32_t FTexture::s_CreatedResourcesCount = 0;
	std::vector<std::shared_ptr<VKE::FTexture>> s_TextureList;

	std::shared_ptr<FTexture> FTexture::Load(const std::string& iTextureName, FMainDevice& iMainDevice, VkFormat Format)
	{
		// Not exist
		if (s_TextureContainer.find(iTextureName) == s_TextureContainer.end())
		{
			auto newTexture = std::make_shared<FTexture>(iTextureName, iMainDevice, Format);

			s_TextureContainer.insert({ iTextureName, newTexture });
			s_TextureList.push_back(newTexture);
			return newTexture;
		}
		else
		{
			return s_TextureContainer.at(iTextureName);
		}
	}

	std::shared_ptr<FTexture> FTexture::Get(int32 ID)
	{
		if (s_TextureList.size() <= 0)
		{
			printf("ERROR: Texture list is empty! Returning Null Texture\n");
			return std::make_shared<FTexture>();
		}
		if (static_cast<size_t>(ID) >= s_TextureList.size())
		{
			printf("ERROR: Texture ID is larger than the list! Returning white texture\n");
			return s_TextureList[0];
		}
		else
		{
			return s_TextureList[ID];
		}
		
	}

	void FTexture::Free()
	{
		s_TextureContainer.clear();
		for (auto& tex : s_TextureList)
		{
			tex->cleanUp();
		}
		s_TextureList.clear();
	}

	FTexture::FTexture(const std::string& iTextureName, FMainDevice& iMainDevice, VkFormat Format)
	{
		pMainDevice = &iMainDevice;
		// Create vkImage, vkMemory
		CreateTextureImage(iTextureName, Format);
		CreateTextureSampler();
	}

	FTexture::FTexture()
	{
		printf("Warning! Default constructor is called, means error happened.\n");
	}

	FTexture::~FTexture()
	{
		--s_CreatedResourcesCount;
		if (s_CreatedResourcesCount == 0)
		{
			printf("All texture assets freed.\n");
		}
	}

	void FTexture::cleanUp()
	{
		vkDestroySampler(pMainDevice->LD, Sampler, nullptr);

		Buffer.CleanUp();
	}

	VkDescriptorImageInfo FTexture::GetImageInfo() const
	{
		VkDescriptorImageInfo Info;
		Info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;			// Image layout when in use
		Info.imageView = Buffer.GetImageView();
		Info.sampler = Sampler;

		return Info;
	}

	int32 FTexture::CreateTextureImage(const std::string& fileName, VkFormat Format)
	{
		// Load image file
		VkDeviceSize ImageSize;

		uint8* ImageData = FileIO::LoadTextureFile(fileName, Width, Height, ImageSize);
		if (!ImageData)
		{
			return -1;
		}

		// Create staging buffer to hold loaded data, ready to copy to device
		cBuffer StagingBuffer;
		if (!StagingBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, ImageSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		)) return false;

		// Map memory to the staging buffer
		void * pData = nullptr;
		vkMapMemory(pMainDevice->LD, StagingBuffer.GetMemory(), 0, ImageSize, 0, &pData);
		memcpy(pData, ImageData, static_cast<size_t>(ImageSize));
		vkUnmapMemory(pMainDevice->LD, StagingBuffer.GetMemory());

		// Free allocated memory for loading textures
		FileIO::freeLoadedTextureData(ImageData);

		// 1. Create image to hold final texture
		if (!Buffer.Init(pMainDevice, Width, Height, Format, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,								// Destination of transfer, and also a texture sampler
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_ASPECT_COLOR_BIT
			))
		{
			return -1;
		}

		// 2. COPY DATA TO THE IMAGE
		// Transition image to be DST for copy operation
		TransitionImageLayout(pMainDevice->LD, pMainDevice->graphicQueue, pMainDevice->GraphicsCommandPool, Buffer.GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Actual copy command
		CopyImageBuffer(pMainDevice->LD, pMainDevice->graphicQueue, pMainDevice->GraphicsCommandPool, StagingBuffer.GetvkBuffer(), Buffer.GetImage(), Width, Height);

		// Transition image to be shader readable for shader usage
		TransitionImageLayout(pMainDevice->LD, pMainDevice->graphicQueue, pMainDevice->GraphicsCommandPool, Buffer.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// 3. Clean up staging buffer parts
		StagingBuffer.cleanUp();

		TextureID = s_CreatedResourcesCount++;
		return TextureID;
	}

	void FTexture::CreateTextureSampler()
	{
		VkSamplerCreateInfo SamplerCreateInfo = {};
		SamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		SamplerCreateInfo.magFilter = VK_FILTER_LINEAR;						// magnified filtering
		SamplerCreateInfo.minFilter = VK_FILTER_LINEAR;						// minified filtering
		SamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in U (x) direction
		SamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in V (y) direction
		SamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;	// How to handle texture wrap in W (z) direction
		SamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;	// Border is black
		SamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;				// Normalized coordinate, the value of the coordinate is (0, 1), if true, (0, image size)
		SamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;		// Mipmap filtering
		SamplerCreateInfo.mipLodBias = 0.0f;								// LOD Bias for mipmap
		SamplerCreateInfo.minLod = 0.0f;									// Min / Max LOD to pick mip-level
		SamplerCreateInfo.maxLod = 0.0f;
		SamplerCreateInfo.anisotropyEnable = VK_TRUE;						// Anisotropy filtering enable, anti-aliasing technique
		SamplerCreateInfo.maxAnisotropy = 16;								// Anisotropy sample level

		VkResult Result = vkCreateSampler(pMainDevice->LD, &SamplerCreateInfo, nullptr, &Sampler);
		RESULT_CHECK(Result, "Fail to create sampler.");
	}

}