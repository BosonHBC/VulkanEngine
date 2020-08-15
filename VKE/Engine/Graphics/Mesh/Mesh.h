#pragma once

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vector>
#include "BufferFormats.h"
#include "Utilities.h"
#include "Buffer/Buffer.h"
#include "Descriptors/DescriptorSet.h"
#include <memory>

namespace VKE
{

	class cMesh
	{
	public:
		// Load asset
		static std::shared_ptr<cMesh> Load(const std::string& iMeshName, FMainDevice& iMainDevice,
			VkQueue TransferQueue, VkCommandPool TransferCommandPool,
			const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices);
		// Free all assets
		static void Free();
		static uint32_t s_CreatedResourcesCount;

		cMesh() = delete;
		cMesh(const cMesh& i_other) = delete;
		cMesh& operator = (const cMesh& i_other) = delete;
		~cMesh();

		cMesh(FMainDevice& iMainDevice, 
			VkQueue TransferQueue, VkCommandPool TransferCommandPool,
			const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices);

		void cleanUp();
		void CreateDescriptorSet(VkDescriptorPool SamplerDescriptorPool);

		uint32_t GetVertexCount() const { return VertexCount; }
		VkBuffer GetVertexBuffer() const { return VertexBuffer.GetvkBuffer(); }

		uint32_t GetIndexCount() const { return IndexCount; }
		VkBuffer GetIndexBuffer() const { return IndexBuffer.GetvkBuffer(); }

		void SetMaterialID(int MatID) { MaterialID = MatID; }
		int GetMaterialID() const {	return MaterialID; }
		const VkDescriptorSet& GetDescriptorSet() { return SamplerDescriptorSet.GetDescriptorSet(); }

	private:
		int MaterialID = 0;
		
		uint32_t VertexCount, IndexCount;
		cBuffer VertexBuffer, IndexBuffer;
		
		FMainDevice* pMainDevice;
		cDescriptorSet SamplerDescriptorSet;	// @TODO: Should be put in Material class

		bool createVertexBuffer(const std::vector<FVertex>& iVertices, VkQueue TransferQueue, VkCommandPool TransferCommandPool);
		bool createIndexBuffer(const std::vector<uint32_t>& iIndices, VkQueue TransferQueue, VkCommandPool TransferCommandPool);
		
	};
}


