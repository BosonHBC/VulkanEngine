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
	class cModel
	{
	public:
		static std::vector<std::string> LoadMaterials(const aiScene* scene);
		static std::vector < std::shared_ptr<cMesh> > LoadNode(const std::string& iFileName, const FMainDevice& MainDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, aiNode* Node, const aiScene* Scene, const std::vector<int>& MatToTex);
		static std::shared_ptr<cMesh> LoadMesh(const std::string& iFileName, const FMainDevice& MainDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, aiMesh* Mesh, const aiScene* Scene, const std::vector<int>& MatToTex);
		
		cModel() = delete;
		cModel(std::shared_ptr<cMesh> iMesh) { MeshList.push_back(iMesh); }
		cModel(const std::vector<std::shared_ptr<cMesh>>& iMeshList) : MeshList(iMeshList) {}
		~cModel() {};

		void cleanUp();

		/** Getters / Setters */
		size_t GetMeshCount() const { return MeshList.size(); }
		std::shared_ptr<cMesh> GetMesh(size_t idx) { assert(idx < GetMeshCount()); return MeshList[idx]; }

		cTransform Transform;
	protected:
		std::vector<std::shared_ptr<cMesh>> MeshList;
		
	};
}