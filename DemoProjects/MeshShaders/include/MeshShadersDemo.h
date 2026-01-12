#pragma once

#include <RenderEngine.h>
#include <Mesh.h>
#include <MeshPipelineState.h>

#include "../assets/shaders/Shared.h"

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class MeshShadersDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
		void BuildInstanceBuffer();
		void BindResources();
		bool ReadMeshlet(const char*				   filePath,
						 std::vector<Meshlet>&		   meshlets,
						 std::vector<struct CullData>& cull_data,
						 std::vector<uint32_t>&		   meshlet_vertices,
						 std::vector<uint32_t>&		   meshlet_triangles_packed);

	private:
		std::unique_ptr<Mesh> m_Model[MAX_LOD_LEVEL];
		std::shared_ptr<BufferD3D12> m_Meshlet[MAX_LOD_LEVEL];
		std::shared_ptr<BufferD3D12> m_MeshletVertices[MAX_LOD_LEVEL];
		std::shared_ptr<BufferD3D12> m_MeshletTris[MAX_LOD_LEVEL];
		std::shared_ptr<BufferD3D12> m_MeshletData;
		std::shared_ptr<BufferD3D12> m_InstanceBuffer;

		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_DispatchDataBuffer;

		MeshPipelineState m_Pso;

		uint32 m_LodCount;
		uint32 m_InstanceCount;
		XMUINT3 m_GridSize;
		UINT m_RenderMode = 0;
	};
}