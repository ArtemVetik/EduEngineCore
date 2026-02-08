#pragma once
#include "framework.h"

#include <Skybox.h>
#include <CSMRendering.h>

namespace EduEngine
{
	class RENDERTECHNIQUES_API ReflectionProbe
	{
	public:
		enum Flags : uint32
		{
			CREATE_IRRADIANCE_MAP = 1 << 0,
			CREATE_PREFILTERED_MAP = 1 << 1,
		};

		struct Settings
		{
			uint16 TextureSize = 512;
			uint32 Flags = 0;
		};

	public:
		ReflectionProbe(RenderDeviceD3D12* device, DeviceContext* context, Settings Settings);

		void Bake(DeviceContext* context, Skybox* skybox, Light* light, PBRPrepass* pbrPrepass, RenderObject* renderObjects, uint32 objectsNum);

		std::shared_ptr<TextureD3D12> GetIrradianceMap() const { return m_IrradianceMap; }
		std::shared_ptr<TextureD3D12> GetPrefilteredMap() const { return m_PrefilteredMap; }

	private:
		void InitializeTextures(RenderDeviceD3D12* device, DeviceContext* context);

		static constexpr uint16 IRRADIANCE_MAP_SIZE = 32;

	private:
		PipelineState m_PSO;
		std::shared_ptr<ShaderBinder> m_Binder;

		std::unique_ptr<CSMRendering> m_CSMRendering;

		std::shared_ptr<TextureD3D12> m_ReflectionCube;
		std::shared_ptr<TextureD3D12> m_IrradianceMap;
		std::shared_ptr<TextureD3D12> m_PrefilteredMap;
		std::shared_ptr<TextureD3D12> m_DepthBuffer;

		std::shared_ptr<DynamicUploadBuffer> m_ObjBuffer;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffer;

		XMFLOAT3 m_Center;
		XMFLOAT2 m_BoxSize;

		Settings m_Settings;
	};
}