#pragma once
#include <d3d12.h>
#include <SimpleMath.h>

namespace EduEngine
{
	struct RENDERTECHNIQUES_API alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) Light
	{
	public:
		enum Type
		{
			Directional = 0,
			Point = 1,
			Spotlight = 2
		};

		Type LightType = Type::Directional;
		float Strength = 3;
		DirectX::XMFLOAT2 Padding = { 0, 0 };
		DirectX::XMFLOAT3 Color = { 10.0f, 10.0f, 10.0f };
		float FalloffStart = 1.04f;							  // point/spot light only
		DirectX::XMFLOAT3 Direction = { -1.0f, -4.0f, -1.0f };// directional/spot light only
		float FalloffEnd = 10.0f;							  // point/spot light only
		DirectX::XMFLOAT3 Position = { 0.0f, 400.0f, 0.0f };  // point/spot light only
		float SpotPower = 64.0f;							  // spot light only
	};
}