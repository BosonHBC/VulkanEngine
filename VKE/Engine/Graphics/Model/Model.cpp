#include "Model.h"
#include "Mesh/Mesh.h"

#include "assimp/Importer.hpp"
#include <assimp/scene.h>
#include "assimp/postprocess.h"
namespace VKE
{

	std::vector<std::string> cModel::LoadMaterials(const aiScene* scene)
	{
		std::vector<std::string> TextureList(scene->mNumMaterials);

		// Go through each material and copy its texture file name
		for (size_t i = 0; i < scene->mNumMaterials; ++i)
		{
			aiMaterial* Material = scene->mMaterials[i];

			TextureList[i] = "";

			// Check for a diffuse texture
			if (Material->GetTextureCount(aiTextureType_DIFFUSE))
			{
				aiString path;
				if (Material->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS)
				{
					// Cut off any directory information already present
					std::string pathStr = std::string(path.data);
					int idx = pathStr.rfind("\\");
					TextureList[i] = pathStr.substr(idx + 1);
				}
			}
		}
		return TextureList;
	}

	std::vector < std::shared_ptr<cMesh> > cModel::LoadNode(const std::string& iFileName, FMainDevice& MainDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, aiNode* Node, const aiScene* Scene, const std::vector<int>& MatToTex)
	{
		std::vector<std::shared_ptr<cMesh>> MeshList;

		// Go through each mesh at this node and create it, then add it to our mesh list
		for (size_t i = 0; i < Node->mNumMeshes; ++i)
		{
			// Load mesh here
			MeshList.push_back(LoadMesh(iFileName, MainDevice, TransferQueue, TransferCommandPool, Scene->mMeshes[Node->mMeshes[i]], Scene, MatToTex));
		}

		// Go though each node attached to this node and load it
		for (size_t i = 0; i < Node->mNumChildren; ++i)
		{
			std::string ChildName = iFileName + "_" + Node->mChildren[i]->mName.C_Str();
			auto newList = LoadNode(ChildName, MainDevice, TransferQueue, TransferCommandPool, Node->mChildren[i], Scene, MatToTex);
			MeshList.insert(MeshList.end(), newList.begin(), newList.end());
		}

		return MeshList;
	}

	std::shared_ptr<cMesh> cModel::LoadMesh(const std::string& iFileName, FMainDevice& MainDevice, VkQueue TransferQueue, VkCommandPool TransferCommandPool, aiMesh* Mesh, const aiScene* Scene, const std::vector<int>& MatToTex)
	{
		std::vector<FVertex> Vertices;
		std::vector<uint32_t> Indices;

		Vertices.resize(Mesh->mNumVertices);

		for (size_t i = 0; i < Vertices.size(); ++i)
		{
			Vertices[i].Position = { Mesh->mVertices[i].x,Mesh->mVertices[i].y, Mesh->mVertices[i].z };

			Vertices[i].Color = { Mesh->mNormals[i].x,Mesh->mNormals[i].y, Mesh->mNormals[i].z };

			// Check first set of texture coordinate
			if (Mesh->mTextureCoords[0])
			{
				Vertices[i].TexCoord = { Mesh->mTextureCoords[0][i].x, Mesh->mTextureCoords[0][i].y };
			}
			else
			{
				Vertices[i].TexCoord = { 0.0f, 0.0f };
			}
		}

		// Iterate over indices through faces and copy across
		for (size_t i = 0; i < Mesh->mNumFaces; ++i)
		{
			// Get a face
			aiFace& Face = Mesh->mFaces[i];
			// Go through face's indices and add to list
			Indices.reserve(Indices.capacity() + Face.mNumIndices);
			for (size_t j = 0; j < Face.mNumIndices; ++j)
			{
				Indices.push_back(Face.mIndices[j]);
			}
		}

		// Create new mesh with details
		std::shared_ptr<cMesh> NewMesh = cMesh::Load(iFileName, MainDevice, TransferQueue, TransferCommandPool, Vertices, Indices);
		int MaterialID = MatToTex[Mesh->mMaterialIndex];

		NewMesh->SetMaterialID(MaterialID);

		return NewMesh;
	}

	void cModel::cleanUp()
	{
		for (auto mesh : MeshList)
		{
			mesh->cleanUp();
		}
		MeshList.clear();
	}

}