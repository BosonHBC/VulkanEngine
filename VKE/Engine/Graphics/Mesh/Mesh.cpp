#include "Mesh.h"

#include "Texture/Texture.h"
#include <map>


namespace VKE
{
	std::map<std::string, std::shared_ptr<VKE::cMesh>> s_MeshContainer;
	uint32_t cMesh::s_CreatedResourcesCount = 0;

	std::shared_ptr<cMesh> cMesh::Load(const std::string& iMeshName, FMainDevice& iMainDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices)
	{
		// Not exist
		if (s_MeshContainer.find(iMeshName) == s_MeshContainer.end())
		{
			auto newMesh = std::make_shared<cMesh>(iMainDevice, TransferQueue, TransferCommandPool, iVertices, iIndices);

			s_MeshContainer.insert({ iMeshName, newMesh });
			return newMesh;
		}
		else
		{
			return s_MeshContainer.at(iMeshName);
		}
	}

	void cMesh::Free()
	{
		s_MeshContainer.clear();
	}

	cMesh::~cMesh()
	{
		--s_CreatedResourcesCount;
		if (s_CreatedResourcesCount == 0)
		{
			printf("All mesh assets freed.\n");
		}
	}

	cMesh::cMesh(FMainDevice& iMainDevice,
		VkQueue TransferQueue, VkCommandPool TransferCommandPool,
		const std::vector<FVertex>& iVertices, const std::vector<uint32_t>& iIndices) : SamplerDescriptorSet(&iMainDevice)
	{
		
		VertexCount = iVertices.size();
		IndexCount = iIndices.size();
		pMainDevice = &iMainDevice;
		createVertexBuffer(iVertices, TransferQueue, TransferCommandPool);
		createIndexBuffer(iIndices, TransferQueue, TransferCommandPool);

		++s_CreatedResourcesCount;
	}

	void cMesh::cleanUp()
	{
		SamplerDescriptorSet.cleanUp();
		VertexBuffer.cleanUp();
		IndexBuffer.cleanUp();
	}

	void cMesh::CreateDescriptorSet(VkDescriptorPool SamplerDescriptorPool)
	{
		cTexture* Tex = cTexture::Get(MaterialID).get();
		if (!Tex)
		{
			printf("Invalid Texture\n");
			Tex = cTexture::Get(0).get();
		}
		// @TODO:
		// In the future, if the material has multiple textures, there should be a material class handle the descriptor creation
		VkDescriptorImageInfo ImageInfo = Tex->GetImageInfo();

		// This is a texture, should be shader read only
		SamplerDescriptorSet.CreateImageBufferDescriptor(&Tex->GetImageBuffer(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, ImageInfo.imageLayout, ImageInfo.sampler);
		SamplerDescriptorSet.CreateDescriptorSetLayout(FirstPass_frag); 
		SamplerDescriptorSet.AllocateDescriptorSet(SamplerDescriptorPool);
		SamplerDescriptorSet.BindDescriptorWithSet();
	}

	bool cMesh::createVertexBuffer(const std::vector<FVertex>& iVertices, VkQueue TransferQueue, VkCommandPool TransferCommandPool)
	{
		VkDeviceSize BufferSize = sizeof(FVertex) * iVertices.size();

		// Create temporary buffer to "stage" data before transferring to GPU
		cBuffer StagingBuffer;
		if (!StagingBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// Transfer source buffer
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |		// CPU can interact with the memory
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT		// Allows placement of data straight into buffer after mapping (otherwise would have to specify manually), no need to flush the data
		)) return false;

		// Map memory to the staging buffer
		void * VertexData = nullptr;																	// 1. Create pointer to a point in random memory;
		vkMapMemory(pMainDevice->LD, StagingBuffer.GetMemory(), 0, BufferSize, 0, &VertexData);					// 2. Map the vertex buffer memory to that point
		memcpy(VertexData, iVertices.data(), static_cast<size_t>(BufferSize));							// 3. copy the data
		vkUnmapMemory(pMainDevice->LD, StagingBuffer.GetMemory());												// 4. unmap the vertex buffer memory, if not using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, need to flush

		// Create buffer with TRANSFER_DST_BIT to mark as recipient of transfer data, it is also a vertex buffer
		if (!VertexBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |			// Transfer destination buffer
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,			// Also a vertex buffer
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT		// Only local visible to GPU, not visible on CPU
		)) return false;

		// Copy staging buffer to vertex buffer in GPU
		CopyBuffer(pMainDevice->LD, TransferQueue, TransferCommandPool, StagingBuffer.GetvkBuffer(), VertexBuffer.GetvkBuffer(), BufferSize);

		// Clean up staging buffer parts
		StagingBuffer.cleanUp();
		return true;
	}


	bool cMesh::createIndexBuffer(const std::vector<uint32_t>& iIndices, VkQueue TransferQueue, VkCommandPool TransferCommandPool)
	{
		VkDeviceSize BufferSize = sizeof(uint32_t) * iIndices.size();

		// Create temporary buffer to "stage" data before transferring to GPU
		cBuffer StagingBuffer;
		if (!StagingBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,			// Transfer source buffer
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |		// CPU can interact with the memory
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT		// Allows placement of data straight into buffer after mapping (otherwise would have to specify manually), no need to flush the data
		)) return false;

		// Map memory to the staging buffer
		void * IndexData = nullptr;																		// 1. Create pointer to a point in random memory;
		vkMapMemory(pMainDevice->LD, StagingBuffer.GetMemory(), 0, BufferSize, 0, &IndexData);					// 2. Map the index buffer memory to that point
		memcpy(IndexData, iIndices.data(), static_cast<size_t>(BufferSize));							// 3. copy the data
		vkUnmapMemory(pMainDevice->LD, StagingBuffer.GetMemory());												// 4. unmap the index buffer memory, if not using VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, need to flush

		// Create buffer with TRANSFER_DST_BIT to mark as recipient of transfer data, it is also a index buffer
		if (!IndexBuffer.CreateBufferAndAllocateMemory(pMainDevice->PD, pMainDevice->LD, BufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |			// Transfer destination buffer
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT,			// Also a index buffer
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT		// Only local visible to GPU, not visible on CPU
		)) return false;

		// Copy staging buffer to index buffer in GPU
		CopyBuffer(pMainDevice->LD, TransferQueue, TransferCommandPool, StagingBuffer.GetvkBuffer(), IndexBuffer.GetvkBuffer(), BufferSize);

		// Clean up staging buffer parts
		StagingBuffer.cleanUp();
		return true;
	}
}