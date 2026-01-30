#pragma once
#include <d3d12.h>

namespace EduEngine
{
	enum SponzaGBufferId
	{
		Albedo = 0,
		Normal = 1,
		NumBuffers = 2,
	};

	const DXGI_FORMAT G_BUFFERS[SponzaGBufferId::NumBuffers]
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
	};

	const DXGI_FORMAT ACCUM_BUFFER_FORMAT = DXGI_FORMAT_R32G32B32A32_FLOAT;
}