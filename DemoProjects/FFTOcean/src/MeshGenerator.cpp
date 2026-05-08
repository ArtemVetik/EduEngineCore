#include "MeshGenerator.h"

#include <SimpleMath.h>

using namespace DirectX;

namespace EduEngine
{
	MeshGenerator::MeshGenerator(RenderDeviceD3D12* device, DeviceContext* context, uint32 w, uint32 h, float scale)
	{
		VERIFY_EXPR(w > 1 && h > 1, "");

		std::vector<XMFLOAT3> vertices;
		std::vector<uint32> indices;

		const uint32 m = w - 1;
		const uint32 n = h - 1;

		for (uint32 i = m; ; --i)
		{
			float z = (float)i / m - 0.5f;
			for (int j = 0; j <= n; ++j)
			{
				float x = (float)j / n - 0.5f;
				vertices.emplace_back(x * scale, 0.0f, z * scale);
			}

			if (i == 0)
				break;
		}

		for (uint32_t i = 0; i < m; ++i)
		{
			for (uint32_t j = 0; j < n; ++j)
			{
				indices.push_back(i * (n + 1) + j);
				indices.push_back(i * (n + 1) + j + 1);
				indices.push_back((i + 1) * (n + 1) + j);
				indices.push_back((i + 1) * (n + 1) + j);
				indices.push_back(i * (n + 1) + j + 1);
				indices.push_back((i + 1) * (n + 1) + j + 1);
			}
		}

		m_IndexCount = indices.size();

		m_VertexBuffer = std::make_unique<VertexBufferD3D12>(device, context, vertices.data(), sizeof(XMFLOAT3), static_cast<uint32_t>(vertices.size()));
		m_IndexBuffer = std::make_unique<IndexBufferD3D12>(device, context, indices.data(), sizeof(uint32), static_cast<uint32_t>(indices.size()), DXGI_FORMAT_R32_UINT);
	}

	D3D12_VERTEX_BUFFER_VIEW MeshGenerator::GetVertexBufferView() const
	{
		return m_VertexBuffer->GetView();
	}

	D3D12_INDEX_BUFFER_VIEW MeshGenerator::GetIndexBufferView() const
	{
		return m_IndexBuffer->GetView();
	}
}