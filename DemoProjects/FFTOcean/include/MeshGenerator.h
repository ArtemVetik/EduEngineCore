#pragma once

#include "framework.h"

#include <BufferD3D12.h>

namespace EduEngine
{
	class MeshGenerator
	{
	public:
		MeshGenerator(RenderDeviceD3D12* device, DeviceContext* context, uint32 w, uint32 h, float scale = 100);

		MeshGenerator(const MeshGenerator&) = delete;
		MeshGenerator& operator=(const MeshGenerator&) = delete;
		MeshGenerator(MeshGenerator&&) = delete;
		MeshGenerator& operator=(MeshGenerator&&) = delete;

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;

		uint32 GetIndexCount() const { return m_IndexCount; }

	private:
		std::unique_ptr<VertexBufferD3D12> m_VertexBuffer;
		std::unique_ptr<IndexBufferD3D12> m_IndexBuffer;

		uint32 m_IndexCount;
	};
}