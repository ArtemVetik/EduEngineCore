#pragma once
#include "framework.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <BufferD3D12.h>

namespace EduEngine
{
	struct MeshLoadDesc
	{
		bool CreateVertexSRV = false;
		bool CreateIndexSRV = false;
	};

	class Mesh
	{
	public:
		Mesh(RenderDeviceD3D12* device, DeviceContext* context, const char* filePath);
		~Mesh();

		void Load(const MeshLoadDesc* loadDesc = nullptr);
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

		const aiMesh* GetAiMesh() const { return m_Scene->mMeshes[0]; }

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