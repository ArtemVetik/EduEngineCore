#pragma once
#include <Camera.h>
#include <InputManager.h>

namespace EduEngine
{
	__forceinline void FreeCameraUpdate(const Timer& timer, Camera* camera, float moveSpeed = 150.0f)
	{
		XMVECTOR direction = XMLoadFloat3(&camera->GetLook());
		XMVECTOR lrVector = XMLoadFloat3(&camera->GetRight());
		XMVECTOR upVector = XMLoadFloat3(&camera->GetUp());

		static constexpr float rotateScale = 0.01f;
		static constexpr float rotateLerpSpeed = 20.0f;

		if (InputManager::GetInstance().IsKeyPressed(DIK_LSHIFT))
			moveSpeed *= 2;

		if (InputManager::GetInstance().IsKeyPressed(DIK_W))
			camera->Move(direction * moveSpeed * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_S))
			camera->Move(-direction * moveSpeed * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_A))
			camera->Move(-lrVector * moveSpeed * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_D))
			camera->Move(lrVector * moveSpeed * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_E))
			camera->Move(upVector * moveSpeed * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_Q))
			camera->Move(-upVector * moveSpeed * timer.GetDeltaTime());

		auto mouseState = InputManager::GetInstance().GetMouseState();

		static XMFLOAT2 currentDelta = { 0, 0 };
		static XMFLOAT2 targetDelta = { 0, 0 };

		if ((mouseState.rgbButtons[1] & 0x80) != 0)
		{
			targetDelta.x += mouseState.lX * rotateScale;
			targetDelta.y += mouseState.lY * rotateScale;
		}

		auto Lerp = [](float a, float b, float t) {
			return a + (b - a) * t;
			};

		float prevX = currentDelta.x;
		currentDelta.x = Lerp(currentDelta.x, targetDelta.x, timer.GetDeltaTime() * rotateLerpSpeed);
		camera->RotateY(currentDelta.x - prevX);

		float prevY = currentDelta.y;
		currentDelta.y = Lerp(currentDelta.y, targetDelta.y, timer.GetDeltaTime() * rotateLerpSpeed);
		camera->Pitch(currentDelta.y - prevY);

		camera->Update(timer);
	}

	__forceinline void RotateAroundCenterCameraUpdate(const Timer& timer, Camera* camera)
	{
		const float WHEEL_SCALE = 2.0f;
		static float theta = 0;
		static float phi = 0;
		static float radius = 100.0f;
		static float rotationScale = 0.01f;

		auto mouseState = InputManager::GetInstance().GetMouseState();

		if ((mouseState.rgbButtons[1] & 0x80) != 0)
		{
			theta -= mouseState.lX * rotationScale;
			phi += mouseState.lY * rotationScale;
			phi = std::clamp(phi, -XM_PIDIV2, XM_PIDIV2);
		}

		float deltaZoom = (float)mouseState.lZ / WHEEL_DELTA * WHEEL_SCALE;
		radius = std::clamp(radius - deltaZoom, 2.0f, 200.0f);

		XMVECTOR pos = XMVectorSet
		(
			cos(phi) * cos(theta) * radius,
			sin(phi) * radius,
			cos(phi) * sin(theta) * radius,
			0.0f
		);

		XMVECTOR look = XMVector3Normalize(-pos);
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		if (fabs(XMVectorGetY(look)) > 0.99f)
		{
			up = XMVectorSet(1, 0, 0, 0);
		}

		XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, look));
		XMVECTOR realUp = XMVector3Cross(look, right);

		XMFLOAT3 posF, lookF, rightF, upF;
		XMStoreFloat3(&posF, pos);
		XMStoreFloat3(&lookF, look);
		XMStoreFloat3(&rightF, right);
		XMStoreFloat3(&upF, realUp);

		camera->Setup(posF, lookF, rightF, upF);
		camera->Update(timer);
	}
}