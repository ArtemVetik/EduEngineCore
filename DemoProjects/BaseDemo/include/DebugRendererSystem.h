#pragma once
#include "framework.h"
#include "RenderPasses.h"

#include <DeviceContext.h>
#include <ShaderD3D12.h>

namespace EduEngine
{
	class DebugRendererSystem
	{
	public:
		DebugRendererSystem(RenderDeviceD3D12* pDevice);

		void DrawBoundingBox(const DirectX::BoundingBox& box, DirectX::XMVECTOR color = DirectX::Colors::Green);
		void DrawBoundingBox(const DirectX::BoundingBox& box, DirectX::XMMATRIX transform, DirectX::XMVECTOR color = DirectX::Colors::Green);
		void DrawCapsule(const double& radius, const double& halfHeight, const DirectX::XMVECTOR& color, const DirectX::XMMATRIX& transform, int density);
		void DrawSphere(const double& radius, const DirectX::XMVECTOR& color, const DirectX::XMMATRIX& transform, int density);
		void DrawCircle(const double& radius, const DirectX::XMVECTOR& color, const DirectX::XMMATRIX& transform, int density);
		void DrawArrow(const DirectX::XMFLOAT3& p0, const DirectX::XMFLOAT3& p1, const DirectX::XMVECTOR& color, const DirectX::XMFLOAT3& n);
		void DrawPoint(const DirectX::XMFLOAT3& pos, const float& size, const DirectX::XMVECTOR& color);
		void DrawLine(const DirectX::XMFLOAT3& pos0, const DirectX::XMFLOAT3& pos1, const DirectX::XMVECTOR& color = DirectX::Colors::Green);
		void DrawLine(const DirectX::XMVECTOR& pos0, const DirectX::XMVECTOR& pos1, const DirectX::XMVECTOR& color = DirectX::Colors::Green);
		void DrawPlane(const DirectX::XMFLOAT4& p, const DirectX::XMVECTOR& color, float sizeWidth, float sizeNormal, bool drawCenterCross);
		void DrawSpotLight(const DirectX::XMFLOAT3& pos0, const DirectX::XMFLOAT3& pos1, float radius, int density, const DirectX::XMVECTOR& color = DirectX::Colors::Green);
		void DrawFrustrum(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);

		void Render(DeviceContext* context, DirectX::XMMATRIX mvp, DirectX::XMFLOAT3 cameraPosition);

	private:
		void DrawInfiniteGrid(const DirectX::XMFLOAT3& cameraPosition, int gridSize, int gridLines);

	private:
		struct VertexPointColor
		{
			DirectX::XMFLOAT3 Position;
			DirectX::XMVECTOR Color;

			VertexPointColor(DirectX::XMFLOAT3 position, DirectX::XMVECTOR color) :
				Position(position),
				Color(color)
			{
			}
		};

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		RenderDeviceD3D12* m_Device;
		DebugRenderPass m_RenderPass;
		std::shared_ptr<EduEngine::EduBinding::ShaderBinder> m_RenderPassBinder;
		std::vector<VertexPointColor> m_Lines;
	};
}