#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>

#include "../Utilities.h"

namespace VKE
{
	class cMesh
	{
	public:
		cMesh() = delete;
		cMesh(const cMesh& i_other) = delete;
		cMesh& operator = (const cMesh& i_other) = delete;
		~cMesh() {}

		cMesh(const FMainDevice& iMainDevice, const std::vector<FVertex>& iVertices);

		void cleanUpVertexBuffer();

		uint32_t GetVertexCount() const { return VertexCount; }
		VkBuffer GetVertexBuffer() const { return VertexBuffer; }

	private:
		uint32_t VertexCount;
		VkBuffer VertexBuffer;
		VkDeviceMemory VertexBufferMemory;

		FMainDevice MainDevice;

		bool createVertexBuffer(const std::vector<FVertex>& iVertices, VkBuffer& oBuffer);
		uint32_t findMemoryTypeIndex(uint32_t AllowedTypes, VkMemoryPropertyFlags Properties);
	};
}


