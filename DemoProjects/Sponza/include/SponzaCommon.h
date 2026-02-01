#pragma once
#include <d3d12.h>

namespace EduEngine
{
	enum SponzaGBufferId
	{
		Albedo = 0,
		Normal = 1,
		MetalRoughAo = 2,
		NumBuffers = 3,
	};

	const DXGI_FORMAT ACCUM_BUFFER_FORMAT = DXGI_FORMAT_R32G32B32A32_FLOAT;
}