#include "Mesh.h"

#include <StringUtils.h>
#include <filesystem>
#include <assimp/GltfMaterial.h>

namespace EduEngine
{
	Mesh::Mesh(RenderDeviceD3D12* device, DeviceContext* context, const char* filePath) :
		m_Device(device),
		m_Context(context),
		m_FilePath(filePath),
		m_Scene(nullptr),
		m_RefCount(0)
	{
		m_ORMTextureGen = std::make_unique<ORMTextureGenerator>(device);
	}

	Mesh::~Mesh()
	{
		m_RefCount = 0;

		for (size_t i = 0; i < m_Scene->mNumMeshes; i++)
		{
			m_VertexBuffers[i].reset();
			m_IndexBuffers[i].reset();
		}

		for (size_t i = 0; i < PBR_TEXTURE_NUM; i++)
			m_Textures[i].clear();

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

		VERIFY_EXPR(loadDesc.VertexInput != 0, "MeshLoadDesc::VertexInput can not be zero");

		UINT flags = aiProcessPreset_TargetRealtime_Fast | aiProcess_ConvertToLeftHanded | aiProcess_PreTransformVertices;

		if (loadDesc.Flags & MESH_LOAD_FLAG_GEN_BOUNDING_BOX)
			flags |= aiProcess_GenBoundingBoxes;

		m_Scene = m_AssimpImporter.ReadFile(m_FilePath, flags);

		if (loadDesc.Flags & MESH_LOAD_FLAG_LOAD_TEXTURES)
		{
			for (size_t i = 0; i < PBR_TEXTURE_NUM; i++)
				m_Textures[i].resize(m_Scene->mNumMeshes);
		}

		m_VertexBuffers.resize(m_Scene->mNumMeshes);
		m_IndexBuffers.resize(m_Scene->mNumMeshes);

		LoadMaterials(loadDesc);

		std::vector<BYTE> vertBuff;

		for (uint32 mi = 0; mi < m_Scene->mNumMeshes; mi++)
		{
			aiMesh* mesh = m_Scene->mMeshes[mi];
			aiMaterial* material = m_Scene->mMaterials[mesh->mMaterialIndex];

			float transmissionFactor = 0;
			if (material->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmissionFactor) == AI_SUCCESS && transmissionFactor > 0)
				m_TransmissionIndexes.push_back(mi);
			else
				m_OpaqueIndexes.push_back(mi);

			if (loadDesc.Flags & MESH_LOAD_FLAG_LOAD_TEXTURES)
				LoadPBRTextures(mi, loadDesc);

			uint32 vertexSize = 0;
			UINT vertexInput = loadDesc.VertexInput;
			UINT input = 1;

			while (vertexInput != 0)
			{
				if (loadDesc.VertexInput & input)
					vertexSize += GetVertexInputSize((MESH_LOAD_VERTEX_INPUT)input);

				input <<= 1;
				vertexInput >>= 1;
			}

			vertBuff.resize(vertexSize * mesh->mNumVertices);
			std::vector<uint32> indices32;

			for (uint32 vi = 0; vi < mesh->mNumVertices; vi++)
			{
				uint32 offset = vi * vertexSize;
				if (loadDesc.VertexInput & MESH_LOAD_VERTEX_INPUT_POSITION)
				{
					auto aiVertex = mesh->mVertices[vi];
					memcpy(vertBuff.data() + offset, &aiVertex, sizeof(aiVertex));
					offset += sizeof(aiVertex);
				}
				if (loadDesc.VertexInput & MESH_LOAD_VERTEX_INPUT_NORMAL)
				{
					auto aiNormal = mesh->mNormals[vi];
					memcpy(vertBuff.data() + offset, &aiNormal, sizeof(aiNormal));
					offset += sizeof(aiNormal);
				}
				if (loadDesc.VertexInput & MESH_LOAD_VERTEX_INPUT_TANGENT)
				{
					auto aiTangents = mesh->mTangents ? mesh->mTangents[vi] : aiVector3D();
					memcpy(vertBuff.data() + offset, &aiTangents, sizeof(aiTangents));
					offset += sizeof(aiTangents);
				}
				if (loadDesc.VertexInput & MESH_LOAD_VERTEX_INPUT_TEXCOORD)
				{
					auto aiTexC = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][vi] : aiVector3D();
					auto aiTexC2D = aiVector2D(aiTexC.x, aiTexC.y);
					memcpy(vertBuff.data() + offset, &aiTexC2D, sizeof(aiTexC2D));
					offset += sizeof(aiTexC2D);
				}
				if (loadDesc.VertexInput & MESH_LOAD_VERTEX_INPUT_COLOR)
				{
					auto aiColor = mesh->mColors[0] ? mesh->mColors[0][vi] : aiColor4D(1, 1, 1, 1);
					memcpy(vertBuff.data() + offset, &aiColor, sizeof(aiColor));
				}
			}

			for (size_t fi = 0; fi < mesh->mNumFaces; fi++)
			{
				for (size_t k = 0; k < mesh->mFaces[fi].mNumIndices; k += 3)
				{
					indices32.push_back(mesh->mFaces[fi].mIndices[k + 2]);
					indices32.push_back(mesh->mFaces[fi].mIndices[k]);
					indices32.push_back(mesh->mFaces[fi].mIndices[k + 1]);
				}
			}

			m_VertexBuffers[mi] = std::make_shared<VertexBufferD3D12>(m_Device, m_Context, vertBuff.data(),
				vertexSize, mesh->mNumVertices);
			m_IndexBuffers[mi] = std::make_shared<IndexBufferD3D12>(m_Device, m_Context, indices32.data(),
				sizeof(uint32), (UINT)indices32.size(), DXGI_FORMAT_R32_UINT);

			if (loadDesc.Flags & MESH_LOAD_FLAG_CREATE_VERTEX_SRV)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = mesh->mNumVertices;
				srvDesc.Buffer.StructureByteStride = vertexSize;

				m_VertexBuffers[mi]->CreateSRV(&srvDesc);
			}

			if (loadDesc.Flags & MESH_LOAD_FLAG_CREATE_INDEX_SRV)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = indices32.size();
				srvDesc.Buffer.StructureByteStride = sizeof(uint32);

				m_IndexBuffers[mi]->CreateSRV(&srvDesc);
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

			for (size_t i = 0; i < PBR_TEXTURE_NUM; i++)
				m_Textures[i].clear();

			m_AssimpImporter.FreeScene();
			m_Scene = nullptr;
		}

	}

	int Mesh::GetVertexCount(uint32 meshIdx) const
	{
		return m_Scene->mMeshes[meshIdx]->mNumVertices;
	}

	int Mesh::GetIndexCount(uint32 meshIdx) const
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

	void Mesh::LoadMaterials(const MeshLoadDesc& loadDesc)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = 0;
		desc.Width = sizeof(Material) * m_Scene->mNumMaterials;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = m_Scene->mNumMaterials;
		srvDesc.Buffer.StructureByteStride = sizeof(Material);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		m_Materials = std::make_shared<BufferD3D12>(m_Device, m_Context, desc, QueueId::Direct);
		m_Materials->SetName(L"MeshMaterialsBuffer");

		std::vector<Material> materials(m_Scene->mNumMaterials);

		for (uint32 i = 0; i < m_Scene->mNumMaterials; i++)
		{
			Material data = {};
			aiMaterial* material = m_Scene->mMaterials[i];

			material->Get(AI_MATKEY_BASE_COLOR, data.BaseColorFactor);
			material->Get(AI_MATKEY_ROUGHNESS_FACTOR, data.RoughnessFactor);
			material->Get(AI_MATKEY_METALLIC_FACTOR, data.MetallicFactor);
			material->Get(AI_MATKEY_TRANSMISSION_FACTOR, data.TransmissionFactor);

			materials[i] = data;
		}

		m_Materials->LoadData(m_Context, materials.data());
		m_Materials->CreateSRV(&srvDesc, loadDesc.Flags & MESH_LOAD_FLAG_CPU_MATERIALS_SRV);
	}

	void Mesh::LoadPBRTextures(uint32 meshIdx, const MeshLoadDesc& loadDesc)
	{
		namespace fs = std::filesystem;

		aiMesh* mesh = m_Scene->mMeshes[meshIdx];
		aiMaterial* material = m_Scene->mMaterials[mesh->mMaterialIndex];

		auto CreateTexture = [&](aiString texPath, PBR_TEXTURE_TYPE type)
			{
				fs::path p(texPath.C_Str());
				std::string texName = p.stem().string();
				texName = loadDesc.TextureBasePath + texName + loadDesc.TextureExt;
				if (fs::exists(texName))
				{
					m_Textures[type][meshIdx] = std::make_unique<Texture>(m_Device, loadDesc.TextureLoadDesc);
					m_Textures[type][meshIdx]->Load(ToWString(texName).c_str(), m_Context);
				}
			};

		// Base Color
		{
			aiString texPath;

			if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS ||
				material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
			{
				CreateTexture(texPath, PBR_TEXTURE_BASE_COLOR);
			}
		}

		// Normal Map
		{
			aiString texPath;

			if (material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS)
			{
				CreateTexture(texPath, PBR_TEXTURE_NORMAL_MAP);
			}
		}

		// Metallic Roughness & Ambient Occlusion
		{
			aiString metalnessTexPath;
			aiString roughnessTexPath;
			aiString aoTexPath;

			material->GetTexture(aiTextureType_METALNESS, 0, &metalnessTexPath);
			material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &roughnessTexPath);
			material->GetTexture(aiTextureType_LIGHTMAP, 0, &aoTexPath);

			if (metalnessTexPath == roughnessTexPath && metalnessTexPath == aoTexPath ||
				metalnessTexPath == roughnessTexPath && aoTexPath.length == 0)
			{
				CreateTexture(metalnessTexPath, PBR_TEXTURE_ORM);
			}
			else if (metalnessTexPath == roughnessTexPath)
			{
				fs::path metalRoughPath(metalnessTexPath.C_Str());
				fs::path aoPath(aoTexPath.C_Str());

				std::string metalTexName = metalRoughPath.stem().string();
				metalTexName = loadDesc.TextureBasePath + metalTexName + loadDesc.TextureExt;

				std::string aoTexName = aoPath.stem().string();
				aoTexName = loadDesc.TextureBasePath + aoTexName + loadDesc.TextureExt;

				auto metalRoughTex = TextureD3D12(m_Device, m_Context, ToWString(metalTexName).c_str(), QueueId::Direct);
				auto aoTex = TextureD3D12(m_Device, m_Context, ToWString(aoTexName).c_str(), QueueId::Direct);

				auto outOrm = m_ORMTextureGen->Generate(m_Context, &metalRoughTex, &aoTex);
				m_Textures[PBR_TEXTURE_ORM][meshIdx] = std::make_unique<Texture>(m_Device, std::move(outOrm), loadDesc.TextureLoadDesc);
			}
			else
			{
				// TODO: if assert failed, extend ORMTextureLoader for this case
				ASSERT_FAILED("Unexpected texture combination");
			}
		}

		// Transmission
		{
			aiString texPath;

			if (material->GetTexture(aiTextureType_TRANSMISSION, 0, &texPath) == AI_SUCCESS)
			{
				CreateTexture(texPath, PBR_TEXTURE_TRANSMISSION);
			}
		}
	}

	uint32 Mesh::GetVertexInputSize(MESH_LOAD_VERTEX_INPUT input)
	{
		switch (input)
		{
		case EduEngine::MESH_LOAD_VERTEX_INPUT_COLOR:
			return sizeof(float) * 4;
		case EduEngine::MESH_LOAD_VERTEX_INPUT_POSITION:
		case EduEngine::MESH_LOAD_VERTEX_INPUT_NORMAL:
		case EduEngine::MESH_LOAD_VERTEX_INPUT_TANGENT:
			return sizeof(float) * 3;
		case EduEngine::MESH_LOAD_VERTEX_INPUT_TEXCOORD:
			return sizeof(float) * 2;
		default:
			ASSERT_FAILED("MESH_LOAD_VERTEX_INPUT [", input ,"] does not exist");
			return 0;
		}
	}
}