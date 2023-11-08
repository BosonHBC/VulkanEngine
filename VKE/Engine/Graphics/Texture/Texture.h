#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <memory>
#include "Buffer/ImageBuffer.h"
#include "Utilities.h"

namespace VKE
{
	enum EDefaultTextureID : uint8_t
	{
		White = 0,
		Count = 1,
	};
	class FTexture
	{
	public:
		// Load asset
		static std::shared_ptr<FTexture> Load(const std::string& iTextureName, FMainDevice& iMainDevice, VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM);
		static std::shared_ptr<FTexture> Get(int32 ID);
		// Free all assets
		static void Free();
		static uint32_t s_CreatedResourcesCount;

		FTexture();
		FTexture(const std::string& iTextureName, FMainDevice& iMainDevice, VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM);
		FTexture(const FTexture& Other) = delete;
		FTexture(FTexture&& Other) = delete;
		FTexture& operator = (const FTexture& Other) = delete;
		FTexture& operator = (FTexture&& Other) = delete;

		~FTexture();

		void cleanUp();

		/** Getters */
		VkDescriptorImageInfo GetImageInfo() const;
		int32 GetID() const { return TextureID; }
		FImageBuffer& GetImageBuffer() { return Buffer; }
	protected:
		FMainDevice* pMainDevice = nullptr;
		int32 Width = 0, Height = 0;

		FImageBuffer Buffer;
		VkSampler Sampler;

		int32 CreateTextureImage(const std::string& fileName, VkFormat Format);
		void CreateTextureSampler();

		int32 TextureID = 0;		// Ordered by the time created
	};

}