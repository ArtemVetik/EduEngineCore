#pragma once

#include <RenderEngine.h>
#include <Mesh.h>
#include <MeshPipelineState.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	struct Meshlet
	{
		uint32 VertexOffset;
		uint32 TriangleOffset;
		uint32 VertexCount;
		uint32 TriangleCount;
	};

	class MeshShadersDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
		std::unique_ptr<Mesh> m_Model;
		std::shared_ptr<BufferD3D12> m_Meshlet;
		std::shared_ptr<BufferD3D12> m_MeshletVertices;
		std::shared_ptr<BufferD3D12> m_MeshletTris;

		MeshPipelineState m_Pso;
		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		uint32 m_MeshletCount;
	};
}