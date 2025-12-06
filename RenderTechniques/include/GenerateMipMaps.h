#pragma once
#include "framework.h"

#include "../../ShaderBinding/EduBinding/include/ComputePipelineState.h"

#include <TextureD3D12.h>
#include <BufferD3D12.h>
#include <SimpleMath.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class GenerateMipMaps
	{
	public:
		GenerateMipMaps(RenderDeviceD3D12* device);

		void Generate(DeviceContext* context, std::shared_ptr<TextureD3D12> texture);

	private:
		struct CB
		{
			UINT SrcMipLevel;
			UINT NumMipLevels;
			DirectX::XMFLOAT2 TexelSize;
			UINT NonPowerTwo;
			UINT IsSRGB;
			DirectX::XMUINT2 Padding;
		};

		void GenerateInternal(DeviceContext* context, std::shared_ptr<TextureD3D12> texture, BufferD3D12* cbRes);

		ComputePipelineState m_PSO;
		std::shared_ptr<ShaderBinder> m_Binder;
		RenderDeviceD3D12* m_Device;
	};
}