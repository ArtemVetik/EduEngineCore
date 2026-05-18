#pragma once
#include <EngineTypes.h>
#include <DirectXMath.h>

namespace EduEngine
{
	template <uint32 N>
	class GradientColors
	{
	public:
		GradientColors() {};

		GradientColors(DirectX::XMFLOAT4 Colors[N])
		{
			SetColors(Colors);
		}

		void SetColors(DirectX::XMFLOAT4 Colors[N])
		{
			memcpy(this->Colors, Colors, sizeof(DirectX::XMFLOAT4) * N);
		}

		DirectX::XMFLOAT4 GetColor(float t) const
		{
			if (t <= 0)
				return Colors[0];

			if (t >= 1)
				return Colors[N - 1];

			float scaledT = t * (N - 1);
			int idx0 = static_cast<int>(scaledT);
			int idx1 = idx0 + 1;
			float lerpT = scaledT - idx0;
			const DirectX::XMFLOAT4& c0 = Colors[idx0];
			const DirectX::XMFLOAT4& c1 = Colors[idx1];

			return DirectX::XMFLOAT4(
				c0.x + (c1.x - c0.x) * lerpT,
				c0.y + (c1.y - c0.y) * lerpT,
				c0.z + (c1.z - c0.z) * lerpT,
				c0.w + (c1.w - c0.w) * lerpT
			);
		}

	private:
		DirectX::XMFLOAT4 Colors[N];
	};
}