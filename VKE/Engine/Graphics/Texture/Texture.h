#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <memory>
#include "Utilities.h"

namespace VKE
{
	class cTexture
	{
	public:
		// Load asset
		static std::shared_ptr<cTexture> Load(const std::string& iTextureName, FMainDevice& iMainDevice, VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM);
		static std::shared_ptr<cTexture> Get(int ID);
		// Free all assets
		static void Free();
		static uint32_t s_CreatedResourcesCount;

		cTexture(const std::string& iTextureName, FMainDevice& iMainDevice, VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM);
		~cTexture();

		void cleanUp();
		VkDescriptorImageInfo GetImageInfo();
		int GetID() const { return TextureID; }
	protected:
		FMainDevice* pMainDevice;
		int Width, Height;

		VkImageLayout ImageLayout;
		VkImage Image;
		VkDeviceMemory TexImageMemory;
		VkImageView ImageView;
		VkSampler Sampler;

		int createTextureImage(const std::string& fileName, VkFormat Format);
		void createTextureSampler();
		
		int TextureID;		// Ordered by the time created
	};

}