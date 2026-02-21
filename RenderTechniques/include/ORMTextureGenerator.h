#pragma once
#include "framework.h"

#include <RenderFeatures.h>
#include <TextureD3D12.h>
#include <ComputePipelineState.h>

namespace EduEngine
{
	class RENDERTECHNIQUES_API ORMTextureGenerator
	{
	public:
		ORMTextureGenerator(RenderDeviceD3D12* device, QueueMask queueMask = QueueId::Direct);

		std::unique_ptr<TextureD3D12> Generate(DeviceContext* context,
											   TextureD3D12* metalRoughness,
											   TextureD3D12* ao);

	private:
		std::shared_ptr<ComputePipelineState> BuildPSO();

	private:
		PSOEntry m_PsoEntry;
		std::shared_ptr<ShaderBinder> m_Binder;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		RenderDeviceD3D12* m_Device;
		QueueMask m_QueueMask;
	};
}