#pragma once
#include "framework.h"

#include <Skybox.h>
#include <CSMRendering.h>

namespace EduEngine
{
	struct RENDERTECHNIQUES_API ReflectionProbesData
	{
		XMFLOAT3 Position;
		XMFLOAT3 BoxExtents;
		UINT IrradianceMapIdx;
		UINT PrefilteredMapIdx;
	};

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
			uint32 Flags = ~0;
		};

	public:
		ReflectionProbe(RenderDeviceD3D12* device, DeviceContext* context, Settings settings);

		ReflectionProbe(const ReflectionProbe&) = delete;
		ReflectionProbe(ReflectionProbe&&) = delete;
		ReflectionProbe& operator = (const ReflectionProbe&) = delete;
		ReflectionProbe& operator = (ReflectionProbe&&) = delete;

		void Bake(DeviceContext* context,
				  PBRPrepass* pbrPrepass,
				  Skybox* skybox,
				  Light* lights,
				  uint32 numLights,
				  RenderObject* renderObjects,
				  uint32 objectsNum);

		void SetCenter(XMFLOAT3 center) { m_Center = center; }
		void SetExtents(XMFLOAT3 extents) { m_Extents = extents; }

		XMFLOAT3 GetCenter() const { return m_Center; }
		XMFLOAT3 GetExtents() const { return m_Extents; }

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
		XMFLOAT3 m_Extents;

		Settings m_Settings;
	};

	class RENDERTECHNIQUES_API ReflectionProbesManager
	{
	public:
		ReflectionProbesManager(RenderDeviceD3D12* device, DeviceContext* context);
		~ReflectionProbesManager();

		ReflectionProbesManager(const ReflectionProbesManager&) = delete;
		ReflectionProbesManager(ReflectionProbesManager&&) = delete;
		ReflectionProbesManager& operator = (const ReflectionProbesManager&) = delete;
		ReflectionProbesManager& operator = (ReflectionProbesManager&&) = delete;

		ReflectionProbe* Add(DeviceContext* context, ReflectionProbe::Settings settings, XMFLOAT3 position = {}, XMFLOAT3 extents = {});
		void RemoveAt(uint32 index);

		void RebuildBuffer(DeviceContext* context);

		ReflectionProbe* GetReflectionProbe(uint32 index) const { return m_ReflectionProbes[index].get(); }
		std::shared_ptr<BufferD3D12> GetGPUBuffer() const { return m_GpuBuffer; }
		
		uint32 Count() const { return m_Count; }

	public:
		static constexpr uint32 MAX_REFLECTION_PROBES = 16;

	private:
		std::vector<std::unique_ptr<ReflectionProbe>> m_ReflectionProbes;
		uint32 m_Count;

		std::shared_ptr<BufferD3D12> m_GpuBuffer;
		RenderDeviceD3D12* m_Device;
	};
}