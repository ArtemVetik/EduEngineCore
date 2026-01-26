#include "Mesh.h"
#include "MeshData.h"

#include <StringUtils.h>
#include <filesystem>

namespace EduEngine
{
	Mesh::Mesh(RenderDeviceD3D12* device, DeviceContext* context, const char* filePath) :
		m_Device(device),
		m_Context(context),
		m_FilePath(filePath),
		m_Scene(nullptr),
		m_RefCount(0)
	{

	}

	Mesh::~Mesh()
	{
		m_RefCount = 0;

		for (size_t i = 0; i < m_Scene->mNumMeshes; i++)
		{
			m_VertexBuffers[i].reset();
			m_IndexBuffers[i].reset();
		}

		m_Textures.clear();
		m_AssimpImporter.FreeScene();
		m_Scene = nullptr;
	}

	void Mesh::Load(const MeshLoadDesc loadDesc)
	{
		if (m_RefCount > 0)
		{
			m_RefCount++;
			return;
		}

		UINT flags = aiProcessPreset_TargetRealtime_Fast | aiProcess_ConvertToLeftHanded | aiProcess_PreTransformVertices;

		if (loadDesc.Flags & MESH_LOAD_FLAG_GEN_BOUNDING_BOX)
			flags |= aiProcess_GenBoundingBoxes;

		m_Scene = m_AssimpImporter.ReadFile(m_FilePath, flags);

		if (loadDesc.Flags & MESH_LOAD_FLAG_LOAD_TEXTURES)
			m_Textures.resize(m_Scene->mNumMeshes);

		m_VertexBuffers.resize(m_Scene->mNumMeshes);
		m_IndexBuffers.resize(m_Scene->mNumMeshes);

		for (uint32 i = 0; i < m_Scene->mNumMeshes; i++)
		{
			aiMesh* mesh = m_Scene->mMeshes[i];

			if (loadDesc.Flags & MESH_LOAD_FLAG_LOAD_TEXTURES)
			{
				aiMaterial* material = m_Scene->mMaterials[mesh->mMaterialIndex];
				if (material->GetTextureCount(aiTextureType_DIFFUSE))
				{
					aiString texPath;
					material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);

					namespace fs = std::filesystem;
					fs::path p(texPath.C_Str());
					std::string texName = p.stem().string();
					texName = loadDesc.TextureBasePath + texName + loadDesc.TextureExt;

					m_Textures[i] = std::make_unique<Texture>();
					m_Textures[i]->Load(ToWString(texName).c_str(), m_Device, m_Context);
				}
				else
				{
					int aa = 123;
				}
			}
			else
			{
				int edads = 123;
			}

			MeshData meshData;
			for (int i = 0; i < mesh->mNumVertices; i++)
			{
				auto aiVertex = mesh->mVertices[i];
				auto aiNormal = mesh->mNormals[i];
				auto aiTangents = mesh->mTangents ? mesh->mTangents[i] : aiVector3D();
				auto aiTexC = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D();

				meshData.Vertices.push_back(Vertex(
					{ aiVertex.x, aiVertex.y, aiVertex.z },
					{ aiNormal.x, aiNormal.y, aiNormal.z },
					{ aiTangents.x, aiTangents.y, aiTangents.z },
					{ aiTexC.x, aiTexC.y }
				));
			}

			for (size_t i = 0; i < mesh->mNumFaces; i++)
			{
				for (size_t k = 0; k < mesh->mFaces[i].mNumIndices; k += 3)
				{
					meshData.Indices32.push_back(mesh->mFaces[i].mIndices[k + 2]);
					meshData.Indices32.push_back(mesh->mFaces[i].mIndices[k]);
					meshData.Indices32.push_back(mesh->mFaces[i].mIndices[k + 1]);
				}
			}

			m_VertexBuffers[i] = std::make_shared<VertexBufferD3D12>(m_Device, m_Context, meshData.Vertices.data(),
				sizeof(Vertex), (UINT)meshData.Vertices.size());
			m_IndexBuffers[i] = std::make_shared<IndexBufferD3D12>(m_Device, m_Context, meshData.GetIndices16().data(),
				sizeof(uint16), (UINT)meshData.GetIndices16().size(), DXGI_FORMAT_R16_UINT);

			if (loadDesc.Flags & MESH_LOAD_FLAG_CREATE_VERTEX_SRV)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = meshData.Vertices.size();
				srvDesc.Buffer.StructureByteStride = sizeof(Vertex);

				m_VertexBuffers[i]->CreateSRV(&srvDesc);
			}

			if (loadDesc.Flags & MESH_LOAD_FLAG_CREATE_INDEX_SRV)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = meshData.GetIndices16().size();
				srvDesc.Buffer.StructureByteStride = sizeof(uint16);

				m_IndexBuffers[i]->CreateSRV(&srvDesc);
			}
		}

		m_RefCount = 1;
	}

	void Mesh::Free()
	{
		if (m_RefCount == 0)
			return;

		m_RefCount--;

		if (m_RefCount == 0)
		{
			for (size_t i = 0; i < m_Scene->mNumMeshes; i++)
			{
				m_VertexBuffers[i].reset();
				m_IndexBuffers[i].reset();
			}

			m_Textures.clear();
			m_AssimpImporter.FreeScene();
			m_Scene = nullptr;
		}

	}

	int Mesh::GetVertexCount(uint32 meshIdx)
	{
		return m_Scene->mMeshes[meshIdx]->mNumVertices;
	}

	int Mesh::GetIndexCount(uint32 meshIdx)
	{
		return m_Scene->mMeshes[meshIdx]->mNumFaces * 3;
	}

	void Mesh::GetBoundingBox(aiVector3D& min, aiVector3D& max, uint32 meshIdx) const
	{
		min = m_Scene->mMeshes[meshIdx]->mAABB.mMin;
		max = m_Scene->mMeshes[meshIdx]->mAABB.mMax;
	}

	void Mesh::GetBoundingSphere(aiVector3D& center, float& radius, uint32 meshIdx) const
	{
		aiAABB& aabb = m_Scene->mMeshes[meshIdx]->mAABB;

		center = (aabb.mMin + aabb.mMax) * 0.5f;
		radius = ((aabb.mMax - aabb.mMin) * 0.5f).Length();
	}
}