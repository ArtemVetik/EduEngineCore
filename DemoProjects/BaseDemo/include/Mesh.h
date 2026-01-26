#pragma once
#include "framework.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <BufferD3D12.h>
#include <Texture.h>

namespace EduEngine
{
	enum MESH_LOAD_FLAGS : UINT
	{
		MESH_LOAD_FLAG_CREATE_VERTEX_SRV = 1 << 0,
		MESH_LOAD_FLAG_CREATE_INDEX_SRV = 1 << 1,
		MESH_LOAD_FLAG_GEN_BOUNDING_BOX = 1 << 2,
		MESH_LOAD_FLAG_LOAD_TEXTURES = 1 << 3,
	};

	struct MeshLoadDesc
	{
		UINT Flags = 0;
		const char* TextureBasePath = nullptr;
		const char* TextureExt = nullptr;
	};

	class Mesh
	{
	public:
		Mesh(RenderDeviceD3D12* device, DeviceContext* context, const char* filePath);
		~Mesh();

		void Load(const MeshLoadDesc loadDesc = MeshLoadDesc{});
		void Free();

		int GetRefCount() const { return m_RefCount; }
		const char* GetFilePath() const { return m_FilePath; }

		int GetVertexCount(uint32 meshIdx = 0);
		int GetIndexCount(uint32 meshIdx = 0);

		int GetMeshCount() const { return m_Scene->mNumMeshes; }
		VertexBufferD3D12* GetVertexBuffer(uint32 meshIdx = 0) const { return m_VertexBuffers[meshIdx].get(); }
		IndexBufferD3D12* GetIndexBuffer(uint32 meshIdx = 0) const { return m_IndexBuffers[meshIdx].get(); }

		std::shared_ptr<VertexBufferD3D12> GetVertexBufferShared(uint32 meshIdx = 0) const { return m_VertexBuffers[meshIdx]; }
		std::shared_ptr<IndexBufferD3D12> GetIndexBufferShared(uint32 meshIdx = 0) const { return m_IndexBuffers[meshIdx]; }

		Texture* GetTexture(uint32 meshIdx = 0) const { return m_Textures[meshIdx].get(); }

		void GetBoundingBox(aiVector3D& min, aiVector3D& max, uint32 meshIdx = 0) const;
		void GetBoundingSphere(aiVector3D& center, float& radius, uint32 meshIdx = 0) const;

	private:
		RenderDeviceD3D12* m_Device;
		DeviceContext* m_Context;

		Assimp::Importer m_AssimpImporter;
		const aiScene* m_Scene;

		std::vector<std::unique_ptr<Texture>> m_Textures;
		std::vector<std::shared_ptr<VertexBufferD3D12>> m_VertexBuffers;
		std::vector<std::shared_ptr<IndexBufferD3D12>> m_IndexBuffers;

		const char* m_FilePath;
		int m_RefCount;
	};
}