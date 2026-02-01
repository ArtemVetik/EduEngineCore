#pragma once
#include <TextureD3D12.h>
#include <BufferD3D12.h>
#include <DynamicUploadBuffer.h>
#include <PipelineState.h>
#include <SimpleMath.h>
#include <Camera.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class DeferredPBRLightPass
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
		};

		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) MaterialData
		{
			DirectX::XMFLOAT4 DiffuseAlbedo;
		};

		struct alignas(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) Light
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
			DirectX::XMFLOAT3 Color = { 0.9f, 0.9f, 0.9f };
			float FalloffStart = 1.04f;							 // point/spot light only
			DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f }; // directional/spot light only
			float FalloffEnd = 10.0f;							 // point/spot light only
			DirectX::XMFLOAT3 Position = { 0.0f, 100.0f, 0.0f }; // point/spot light only
			float SpotPower = 64.0f;							 // spot light only
		};

		struct PsoMacros
		{
			uint32 DebugView = 0;
			uint32 PackNormalMethod = 0;
		};

		inline static const char* DebugViews[12] =
		{
			"NONE", "DEBUGVIEW_ROUGHNESS", "DEBUGVIEW_METALLIC", "DEBUGVIEW_AO",
			"DEBUGVIEW_NORMAL", "DEBUGVIEW_DIFFUSE_IBL", "DEBUGVIEW_SPECULAR_IBL",
			"DEBUGVIEW_NDOTV", "DEBUGVIEW_FRESNEL", "DEBUGVIEW_BRDF_Y", "DEBUGVIEW_BRDF_X", "DEBUGVIEW_SSAO"
		};

	public:
		DeferredPBRLightPass(RenderDeviceD3D12* device, DeviceContext* context, DXGI_FORMAT rtFormat);

		void Update(DeviceContext* context, const Camera* camera, Light* lights, uint32 numLights);
		void Render(DeviceContext* context, TextureD3D12* target);

		void RebuildPSO(RenderDeviceD3D12* device, DXGI_FORMAT rtFormat, PsoMacros macros);

		void SetBufferIndexes(DeviceContext* context, const BuffersIndexesData& data);
		void SetMaterial(DeviceContext* context, const MaterialData& data);

	private:
		std::unique_ptr<PipelineState> m_Pso;
		std::shared_ptr<ShaderBinder> m_Binder;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_LightsBuffer;

		std::shared_ptr<BufferD3D12> m_TextureIndexesBuffer;
		std::shared_ptr<BufferD3D12> m_MaterialBuffer;
	};
}