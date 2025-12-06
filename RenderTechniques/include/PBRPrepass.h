#pragma once
#include "framework.h"
#include "Camera.h"

#include "../../ShaderBinding/EduBinding/include/ShaderD3D12.h"
#include "../../ShaderBinding/EduBinding/include/PipelineState.h"

#include <TextureD3D12.h>
#include <BufferD3D12.h>

namespace EduEngine
{
	class PBRPrepass
	{
	public:
		PBRPrepass(RenderDeviceD3D12* device, DeviceContext* context);

		void GenerateTextures(const char* hdrFileName, RenderDeviceD3D12* device, DeviceContext* context);

		void RenderSky(RenderDeviceD3D12* device, DeviceContext* context, Camera* camera);

		void SetSkyTex(std::shared_ptr<TextureD3D12> texture);
		void SetSkyLod(float lod) { m_SkyLod = lod; }

		std::shared_ptr<TextureD3D12> GetHDREnvCubeMap() const { return m_HDRCubeEnvMap; }
		std::shared_ptr<TextureD3D12> GetIrradianceMap() const { return m_IrradianceMap; }
		std::shared_ptr<TextureD3D12> GetPrefilteredMap() const { return m_PrefilteredMap; }
		std::shared_ptr<TextureD3D12> GetBrdfLut() const { return m_BrdfLut; }

		static constexpr uint16 PREFILTERED_MIP_LEVELS = 9;

	private:
		void InitCube(RenderDeviceD3D12* device, DeviceContext* context);
		void InitTextures(RenderDeviceD3D12* device, DeviceContext* context);
		void InitSkyboxPSO(RenderDeviceD3D12* device);

	private:
		static constexpr uint16 ENV_CUBEMAP_SIZE = 512;
		static constexpr uint16 IRRADIANCE_MAP_SIZE = 32;
		static constexpr uint16 PREFILTERED_MAP_SIZE = 256;
		static constexpr uint16 BRDF_LUT_SIZE = 512;

		EduEngine::EduBinding::PipelineState m_PsoSkybox;
		std::shared_ptr<EduEngine::EduBinding::ShaderBinder> m_PsoSkyboxBinder;

		std::shared_ptr<TextureD3D12> m_HDRCubeEnvMap;
		std::shared_ptr<TextureD3D12> m_IrradianceMap;
		std::shared_ptr<TextureD3D12> m_PrefilteredMap;
		std::shared_ptr<TextureD3D12> m_BrdfLut;

		std::shared_ptr<DynamicUploadBuffer> m_SkyboxPassBuff;
		float m_SkyLod;

		std::shared_ptr<IndexBufferD3D12> m_CubeIB;
		std::shared_ptr<VertexBufferD3D12> m_CubeVB;
	};
}