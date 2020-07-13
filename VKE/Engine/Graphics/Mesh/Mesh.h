#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include "BufferFormats.h"
#include "Utilities.h"
#include "Buffer/Buffer.h"
#include <memory>

namespace VKE
{

	class cMesh
	{
	public:
		// Load asset
		static std::shared_ptr<cMesh> Load(const std::string& iMeshName, const FMainDevice& iMainDevice,
			VkQueue TransferQueue, VkCommandPool TransferCommandPool,
			const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices);
		// Free all assets
		static void Free();

		cMesh() = delete;
		cMesh(const cMesh& i_other) = delete;
		cMesh& operator = (const cMesh& i_other) = delete;
		~cMesh();

		cMesh(const FMainDevice& iMainDevice, 
			VkQueue TransferQueue, VkCommandPool TransferCommandPool,
			const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices);

		void cleanUp();

		uint32_t GetVertexCount() const { return VertexCount; }
		VkBuffer GetVertexBuffer() const { return VertexBuffer.GetBuffer(); }

		uint32_t GetIndexCount() const { return IndexCount; }
		VkBuffer GetIndexBuffer() const { return IndexBuffer.GetBuffer(); }


		void SetTextureID(int TexID) { TextureID = TexID; }
		int GetTextureID() const {	return TextureID; }
	private:
		int TextureID = 0;
		
		uint32_t VertexCount, IndexCount;
		cBuffer VertexBuffer, IndexBuffer;
		
		FMainDevice MainDevice;

		bool createVertexBuffer(const std::vector<FVertex>& iVertices, VkQueue TransferQueue, VkCommandPool TransferCommandPool);
		bool createIndexBuffer(const std::vector<uint32_t>& iIndices, VkQueue TransferQueue, VkCommandPool TransferCommandPool);

	};
}


