#pragma once
#include <TextureD3D12.h>
#include <BufferD3D12.h>
#include <DynamicUploadBuffer.h>
#include <PipelineState.h>
#include <SimpleMath.h>
#include <RenderFeatures.h>
#include <CSMRendering.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API DeferredPBRLightPass
	{
	public:
		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) BuffersIndexesData
		{
			UINT AlbedoIdx;
			UINT NormalIdx;
			UINT MetallicRoughAoIdx;
			UINT DepthIdx;
			UINT SsaoMapIdx;
			UINT IrradianceMapIdx;
			UINT PrefilteredMapIdx;
			UINT BRDFLutIdx;
			UINT ShadowMapIdx[CSMRendering::MAX_CASCADES];
		};

		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) MaterialData
		{
			DirectX::XMFLOAT4 DiffuseAlbedo;
		};

	public:
		DeferredPBRLightPass(RenderDeviceD3D12* device, DeviceContext* context, DXGI_FORMAT rtFormat);

		void Update(DeviceContext* context, const Camera* camera, Light* lights, uint32 numLights, CSMRendering* csmRendering);
		void Render(DeviceContext* context, TextureD3D12* target);

		void SetBufferIndexes(DeviceContext* context, const BuffersIndexesData& data);
		void SetMaterial(DeviceContext* context, const MaterialData& data);

	private:
		std::shared_ptr<PipelineStateBase> RebuildPSO();
	
	private:
		PSOEntry m_PsoEntry;
		std::shared_ptr<ShaderBinder> m_Binder;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_LightsBuffer;

		std::shared_ptr<BufferD3D12> m_TextureIndexesBuffer;
		std::shared_ptr<BufferD3D12> m_MaterialBuffer;

		RenderDeviceD3D12* m_Device;
		DXGI_FORMAT m_RtFormat;
	};
}