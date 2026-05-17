#pragma once
#include "framework.h"

#include <ComputePipelineState.h>
#include <PipelineState.h>
#include <TextureD3D12.h>
#include <BufferD3D12.h>
#include <Camera.h>
#include <SimpleMath.h>

using namespace EduEngine::EduBinding;
using namespace DirectX;

namespace EduEngine
{
	struct RENDERTECHNIQUES_API Settings
	{
		float PlanetRadius = 6360000;
		float AtmosphereRadius = 6420000;
		
		float MieG = 0.85f;

		XMFLOAT3 RayleighScatteringCoefficient = { 5.802e-6f, 13.558e-6f, 6.5e-5f };
		XMFLOAT3 RayleighAbsorptionCoefficient = { 0.0f, 0.0f, 0.0f };
		float RayleighScaleHeight = 8000;

		XMFLOAT3 MieScatteringCoefficient = { 3.996e-6f, 3.996e-6f, 3.996e-6f };
		XMFLOAT3 MieAbsorptionCoefficient = { 4.4e-6f, 4.4e-6f, 4.4e-6f };
		float MieScaleHeight = 1200;

		XMFLOAT3 OzoneScatteringCoefficient = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 OzoneAbsorptionCoefficient = { 0.65e-6f, 1.881e-6f, 0.085e-6f };
		
		XMFLOAT3 GroundSpectrumAlbedo = { 0.0f, 0.0f, 0.0f };
	};

	class RENDERTECHNIQUES_API Atmosphere
	{
	public:
		Atmosphere(RenderDeviceD3D12* device, DeviceContext* context, XMFLOAT3 sunDirection);

		void Render(const Camera* camera, XMFLOAT3 sunDirection);

		void UpdateSettings(Settings newSettings, DeviceContext* context);
		Settings GetSettings() const { return m_Settings; };

	private:
		std::shared_ptr<TextureD3D12> m_TransmittanceLut;
		std::shared_ptr<TextureD3D12> m_MultiscatteringLUT;
		std::shared_ptr<TextureD3D12> m_SkyViewLUT;

		std::shared_ptr<BufferD3D12> m_LUTSizeBuffer;
		std::shared_ptr<BufferD3D12> m_AtmosphereParamsBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_DrawPassBuffer;

		std::shared_ptr<VertexBufferD3D12> m_CubeVB;
		std::shared_ptr<IndexBufferD3D12> m_CubeIB;

		std::unique_ptr<ComputePipelineState> m_MultiscatteringLUTPso;
		std::unique_ptr<ComputePipelineState> m_SkyViewLUTPso;
		std::unique_ptr<ComputePipelineState> m_TransmittanceLUTPso;
		PipelineState m_DrawPso;

		std::shared_ptr<ShaderBinder> m_MultiscatteringLUTBinder;
		std::shared_ptr<ShaderBinder> m_SkyViewLUTBinder;
		std::shared_ptr<ShaderBinder> m_TransmittanceLUTBinder;
		std::shared_ptr<ShaderBinder> m_DrawBinder;

		Settings m_Settings;
		DeviceContext* m_Context;
	};
}