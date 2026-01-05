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

	struct CullData
	{
		float sphere_center[3];
		float radius;
		float cone_apex[3];
		signed char cone_axis_s8[3];
		signed char cone_cutoff_s8;
	};

	struct Pass
	{
		XMFLOAT4X4 World;
		XMFLOAT4X4 ViewProj;
		XMFLOAT3 CameraPos;
		float Scale;
		XMFLOAT4 Planes[6];
	};

	struct Instance
	{
		UINT MeshletCount = 0;
		UINT Flags = 0;
		XMUINT2 Padding = { 0, 0 };
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
		std::shared_ptr<BufferD3D12> m_CullData;
		std::shared_ptr<BufferD3D12> m_MeshletVertices;
		std::shared_ptr<BufferD3D12> m_MeshletTris;
		std::shared_ptr<BufferD3D12> m_InstanceBuffer;

		MeshPipelineState m_Pso;
		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		Instance m_InstanceData;
		Pass m_PassData;
	};
}