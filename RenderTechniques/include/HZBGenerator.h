#pragma once
#include "framework.h"

#include <TextureD3D12.h>
#include <ComputePipelineState.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API HZBGenerator
	{
	public:
		HZBGenerator(RenderDeviceD3D12* device);
		void Generate(DeviceContext* context, TextureD3D12* depth);

		void Resize(TextureD3D12* depth);

	private:
		static constexpr uint32 MAX_BATCH_SIZE = 4;

	private:
		std::unique_ptr<TextureD3D12> m_HZBTexture;

		ComputePipelineState m_Pso[MAX_BATCH_SIZE];
		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		DescriptorHeapAllocation m_DepthSrv;

		RenderDeviceD3D12* m_Device;
	};
}