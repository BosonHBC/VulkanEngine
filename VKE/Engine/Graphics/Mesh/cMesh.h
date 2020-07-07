#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include "BufferFormats.h"
#include "Utilities.h"
#include "Buffer/Buffer.h"

namespace VKE
{

	class cMesh
	{
	public:
		cMesh() = delete;
		cMesh(const cMesh& i_other) = delete;
		cMesh& operator = (const cMesh& i_other) = delete;
		~cMesh() {}

		cMesh(const FMainDevice& iMainDevice, 
			VkQueue TransferQueue, VkCommandPool TransferCommandPool,
			const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices);

		void cleanUp();

		uint32_t GetVertexCount() const { return VertexCount; }
		VkBuffer GetVertexBuffer() const { return VertexBuffer.GetBuffer(); }

		uint32_t GetIndexCount() const { return IndexCount; }
		VkBuffer GetIndexBuffer() const { return IndexBuffer.GetBuffer(); }

		void SetModel(glm::mat4 model) { DrawCallData = model; }
		BufferFormats::FDrawCall GetDrawcall() const { return DrawCallData; }
	private:

		uint32_t VertexCount, IndexCount;
		cBuffer VertexBuffer, IndexBuffer;
		
		BufferFormats::FDrawCall DrawCallData;

		FMainDevice MainDevice;

		bool createVertexBuffer(const std::vector<FVertex>& iVertices, VkQueue TransferQueue, VkCommandPool TransferCommandPool);
		bool createIndexBuffer(const std::vector<uint32_t>& iIndices, VkQueue TransferQueue, VkCommandPool TransferCommandPool);

	};
}


