#pragma once
#include "framework.h"

#include <PBRPrepass.h>

namespace EduEngine
{
	class RENDERTECHNIQUES_API Skybox
	{
	public:
		Skybox(const char* hdrFileName, RenderDeviceD3D12* device, DeviceContext* context, PBRPrepass* pbrPrepass, bool cpuTextureHandles = false);
		
		void Render(DeviceContext* context, Camera* camera);

		void RebuildSky(const char* hdrFileName, DeviceContext* context, PBRPrepass* pbrPrepass);

		void SetSky(std::shared_ptr<TextureD3D12> skyTexture);
		void SetLod(float lod) { m_SkyLod = lod; }

		std::shared_ptr<TextureD3D12> GetHDREnvCubeMap() const { return m_HDRCubeEnvMap; }
		std::shared_ptr<TextureD3D12> GetIrradianceMap() const { return m_IrradianceMap; }
		std::shared_ptr<TextureD3D12> GetPrefilteredMap() const { return m_PrefilteredMap; }

	private:
		static constexpr uint16 ENV_CUBEMAP_SIZE = 512;
		static constexpr uint16 IRRADIANCE_MAP_SIZE = 32;

	private:
		PipelineState m_PsoSkybox;
		std::shared_ptr<EduEngine::EduBinding::ShaderBinder> m_PsoSkyboxBinder;

		std::shared_ptr<TextureD3D12> m_HDRCubeEnvMap;
		std::shared_ptr<TextureD3D12> m_IrradianceMap;
		std::shared_ptr<TextureD3D12> m_PrefilteredMap;

		std::shared_ptr<DynamicUploadBuffer> m_SkyboxPassBuff;

		std::shared_ptr<IndexBufferD3D12> m_CubeIB;
		std::shared_ptr<VertexBufferD3D12> m_CubeVB;

		RenderDeviceD3D12* m_Device;
		float m_SkyLod;
		bool m_CpuTextureHandles;
	};
}