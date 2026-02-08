#include "Camera.h"

#include <Asserts.h>

namespace EduEngine
{
	Camera::Camera(UINT width, UINT height) :
		Camera(
			width,
			height,
			60.0f * (3.14f / 180.0f), // fovY
			1.0f,					  // near
			1000.0f,				  // far
			{ 0, 0, -150 },			  // pos
			{ 0, 0, 1 },			  // look
			{ 0, 1, 0}				  // up
		)
	{ }

	Camera::Camera(UINT width, UINT height, float fovY, float nearValue, float farValue, XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 up) :
		m_ScreenWidth(width),
		m_ScreenHeight(height),
		m_FovY(fovY),
		m_NearValue(nearValue),
		m_FarValue(farValue),
		m_Position(pos),
		m_Look(look),
		m_Up(up),
		m_ViewDirty(false)
	{
		XMVECTOR posV = XMLoadFloat3(&m_Position);
		XMVECTOR lookV = XMLoadFloat3(&m_Look);
		XMVECTOR upV = XMLoadFloat3(&m_Up);

		XMStoreFloat3(&m_Right, XMVector3Cross(upV, lookV));

		XMMATRIX V = XMMatrixLookToLH(
			posV,
			lookV,
			upV);
		XMStoreFloat4x4(&m_ViewMatrix, (V));

		SetProjectionMatrix();
	}

	void Camera::UpdateViewport(UINT newWidth, UINT newHeight)
	{
		m_ScreenWidth = newWidth;
		m_ScreenHeight = newHeight;

		SetProjectionMatrix();
	}

	void Camera::UpdateFovY(float fovY)
	{
		m_FovY = fovY;
		
		SetProjectionMatrix();
	}

	void Camera::UpdateNearFar(float nearValue, float farValue)
	{
		m_NearValue = nearValue;
		m_FarValue = farValue;

		SetProjectionMatrix();
	}

	void Camera::Pitch(float angle)
	{
		if (angle == 0)
			return;

		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&m_Right), angle);

		XMStoreFloat3(&m_Up, XMVector3TransformNormal(XMLoadFloat3(&m_Up), R));
		XMStoreFloat3(&m_Look, XMVector3TransformNormal(XMLoadFloat3(&m_Look), R));

		m_ViewDirty = true;
	}

	void Camera::RotateY(float angle)
	{
		if (angle == 0)
			return;

		XMMATRIX R = XMMatrixRotationY(angle);

		XMStoreFloat3(&m_Right, XMVector3TransformNormal(XMLoadFloat3(&m_Right), R));
		XMStoreFloat3(&m_Up, XMVector3TransformNormal(XMLoadFloat3(&m_Up), R));
		XMStoreFloat3(&m_Look, XMVector3TransformNormal(XMLoadFloat3(&m_Look), R));

		m_ViewDirty = true;
	}

	void Camera::Move(XMVECTOR deltaPos)
	{
		XMVECTOR pos = XMLoadFloat3(&m_Position);
		pos += deltaPos;
		XMStoreFloat3(&m_Position, pos);

		m_ViewDirty = true;
	}

	void Camera::Update()
	{
		if (m_ViewDirty)
		{
			ConstructViewMatrix(m_ViewMatrix, m_Right, m_Up, m_Look, m_Position);
			m_ViewDirty = false;
		}
	}

	void Camera::Setup(XMFLOAT3 pos, XMFLOAT3 look, XMFLOAT3 right, XMFLOAT3 up)
	{
		m_Position = pos;
		m_Look = look;
		m_Right = right;
		m_Up = up;
		m_ViewDirty = true;
	}

	XMMATRIX Camera::GetViewProjMatrix() const
	{
		return DirectX::XMLoadFloat4x4(&m_ViewMatrix) * DirectX::XMLoadFloat4x4(&m_ProjectionMatrix);
	}

	void Camera::CalculateLocalBoundingSphere(float n, float f, XMFLOAT4& boundingSphere)
	{
		// https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html

		boundingSphere.x = 0;
		boundingSphere.y = 0;

		float k = sqrtf(1 + (m_ScreenWidth * m_ScreenWidth) / (m_ScreenHeight * m_ScreenHeight)) * tanf(m_FovY * 0.5f);

		VERIFY_EXPR(n <= f, "nearValue must be not greater than farValue");

		if (k * k >= (f - n) / (f + n))
		{
			boundingSphere.z = f;
			boundingSphere.w = f * k;
		}
		else
		{
			boundingSphere.z = 0.5f * (f + n) * (1 + k * k);
			boundingSphere.w = 0.5f * sqrtf
			(
				(f - n) * (f - n) +
				2 * (f * f + n * n) * k * k +
				((f + n) * (f + n)) * k * k * k * k
			);
		}
	}

	void Camera::SetProjectionMatrix()
	{
		m_FovY = std::max(m_FovY, FLT_MIN);
		m_NearValue = std::max(m_NearValue, FLT_MIN);
		m_FarValue = std::max(m_FarValue, m_NearValue + 0.1f);

		auto aspectRatio = ((float)m_ScreenWidth) / ((float)m_ScreenHeight);
		if (aspectRatio == 0)
			aspectRatio = FLT_MAX;

		auto nearWindowHeight = 2.0f * m_NearValue * tanf(0.5f * m_FovY);
		float halfWidth = 0.5f * aspectRatio * nearWindowHeight;
		m_FovX = 2.0f * atan(halfWidth / m_NearValue);

		XMMATRIX P = XMMatrixPerspectiveFovLH(
			m_FovY,
			aspectRatio,
			m_FarValue,
			m_NearValue
		);
		XMStoreFloat4x4(&m_ProjectionMatrix, (P));
	}

	void Camera::ConstructViewMatrix(XMFLOAT4X4& view, XMFLOAT3& right, XMFLOAT3& up, XMFLOAT3& look, XMFLOAT3& pos) const
	{
		XMVECTOR R = XMLoadFloat3(&right);
		XMVECTOR U = XMLoadFloat3(&up);
		XMVECTOR L = XMLoadFloat3(&look);
		XMVECTOR P = XMLoadFloat3(&pos);

		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		R = XMVector3Cross(U, L);

		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&right, R);
		XMStoreFloat3(&up, U);
		XMStoreFloat3(&look, L);

		view(0, 0) = right.x;
		view(1, 0) = right.y;
		view(2, 0) = right.z;
		view(3, 0) = x;

		view(0, 1) = up.x;
		view(1, 1) = up.y;
		view(2, 1) = up.z;
		view(3, 1) = y;

		view(0, 2) = look.x;
		view(1, 2) = look.y;
		view(2, 2) = look.z;
		view(3, 2) = z;

		view(0, 3) = 0.0f;
		view(1, 3) = 0.0f;
		view(2, 3) = 0.0f;
		view(3, 3) = 1.0f;
	}
}