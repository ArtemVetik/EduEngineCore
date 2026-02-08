#pragma once

#include <DirectXColors.h>

#include "framework.h"
#include "Timer.h"

#include "../../Graphics/include/RenderDeviceD3D12.h"

namespace EduEngine
{
	using namespace DirectX;

	class RENDERTECHNIQUES_API Camera
	{
	public:
		Camera(UINT width, UINT height);
		Camera(UINT width, UINT height, float fovY, float nearValue, float farValue, XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 up);

		void UpdateViewport(UINT newWidth, UINT newHeight);
		void UpdateFovY(float fovY);
		void UpdateNearFar(float nearValue, float farValue);

		void Pitch(float angle);
		void RotateY(float angle);
		void Move(XMVECTOR deltaPos);
		void Update();
		void Setup(XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 right, XMFLOAT3 up);

		XMFLOAT4X4 GetViewMatrix() const { return m_ViewMatrix; }
		XMFLOAT4X4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
		XMFLOAT3 GetPosition() const { return m_Position; }
		XMFLOAT3 GetLook() const { return m_Look; }
		XMFLOAT3 GetRight() const { return m_Right; }
		XMFLOAT3 GetUp() const { return m_Up; }
		float GetNear() const { return m_NearValue; }
		float GetFar() const { return m_FarValue; }
		float GetFovY() const { return m_FovY; }
		float GetFovX() const { return m_FovX; }

		XMMATRIX GetViewProjMatrix() const;

		void CalculateLocalBoundingSphere(float nearValue, float farValue, XMFLOAT4& boundingSphere);

	private:
		void SetProjectionMatrix();
		void ConstructViewMatrix(XMFLOAT4X4& view, XMFLOAT3& right, XMFLOAT3& up, XMFLOAT3& look, XMFLOAT3& pos) const;

	private:
		UINT m_ScreenWidth;
		UINT m_ScreenHeight;

		float m_FovY;
		float m_FovX;

		XMFLOAT4X4 m_ViewMatrix;
		XMFLOAT4X4 m_ProjectionMatrix;
		float m_NearValue;
		float m_FarValue;

		XMFLOAT3 m_Position = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 m_Right = { 1.0f, 0.0f, 0.0f };
		XMFLOAT3 m_Up = { 0.0f, 1.0f, 0.0f };
		XMFLOAT3 m_Look = { 0.0f, 0.0f, 1.0f };
		bool m_ViewDirty;
	};
}