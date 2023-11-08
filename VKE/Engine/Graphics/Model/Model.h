#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "Transform/Transform.h"
#include "Mesh/Mesh.h"

struct aiScene;
struct aiNode;
struct aiMesh;
namespace VKE
{
	class FModel
	{
	public:
		static std::vector<std::string> LoadMaterials(const aiScene* scene);
		static std::vector < std::shared_ptr<cMesh> > LoadNode(const std::string& iFileName, FMainDevice& MainDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, aiNode* Node, const aiScene* Scene, const std::vector<int32>& MatToTex);
		static std::shared_ptr<cMesh> LoadMesh(const std::string& iFileName, FMainDevice& MainDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, aiMesh* Mesh, const aiScene* Scene, const std::vector<int32>& MatToTex);
		
		FModel() = delete;
		FModel(std::shared_ptr<cMesh> iMesh) { MeshList.push_back(iMesh); }
		FModel(const std::vector<std::shared_ptr<cMesh>>& iMeshList) : MeshList(iMeshList) {}
		virtual ~FModel() {}

		void cleanUp();

		/** Getters / Setters */
		size_t GetMeshCount() const { return MeshList.size(); }
		std::shared_ptr<cMesh> GetMesh(size_t idx) { assert(idx < GetMeshCount()); return MeshList[idx]; }

		FTransform Transform;
	protected:
		std::vector<std::shared_ptr<cMesh>> MeshList;
		
	};
}