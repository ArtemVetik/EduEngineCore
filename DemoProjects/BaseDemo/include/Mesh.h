#pragma once
#include "framework.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <BufferD3D12.h>

namespace EduEngine
{
	enum MESH_LOAD_FLAGS : UINT
	{
		MESH_LOAD_FLAG_CREATE_VERTEX_SRV = 1 << 0,
		MESH_LOAD_FLAG_CREATE_INDEX_SRV = 1 << 1,
		MESH_LOAD_FLAG_GEN_BOUNDING_BOX = 1 << 2,
	};

	struct MeshLoadDesc
	{
		UINT Flags = 0;
	};

	class Mesh
	{
	public:
		Mesh(RenderDeviceD3D12* device, DeviceContext* context, const char* filePath);
		~Mesh();

		void Load(const MeshLoadDesc loadDesc = MeshLoadDesc{});
		void Free();

		void UpdateFilePath(const char* filePath);

		int GetRefCount() const { return m_RefCount; }
		const char* GetFilePath() const { return m_FilePath; }

		int GetVertexCount();
		int GetIndexCount();

		VertexBufferD3D12* GetVertexBuffer() const { return m_VertexBuffer.get(); }
		IndexBufferD3D12* GetIndexBuffer() const { return m_IndexBuffer.get(); }

		std::shared_ptr<VertexBufferD3D12> GetVertexBufferShared() const { return m_VertexBuffer; }
		std::shared_ptr<IndexBufferD3D12> GetIndexBufferShared() const { return m_IndexBuffer; }

		void GetBoundingBox(aiVector3D& min, aiVector3D& max) const;
		void GetBoundingSphere(aiVector3D& center, float& radius) const;

	private:
		RenderDeviceD3D12* m_Device;
		DeviceContext* m_Context;

		Assimp::Importer m_AssimpImporter;
		const aiScene* m_Scene;
		std::shared_ptr<VertexBufferD3D12> m_VertexBuffer;
		std::shared_ptr<IndexBufferD3D12> m_IndexBuffer;

		const char* m_FilePath;
		int m_RefCount;
	};
}