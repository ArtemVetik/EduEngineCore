#pragma once
#include "framework.h"

#include <Camera.h>
#include <PipelineState.h>
#include <TextureD3D12.h>
#include <BufferD3D12.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API IBLRendering
	{
	public:
		IBLRendering(RenderDeviceD3D12* device, DeviceContext* context);

		void GenerateCubemapFromHDR(RenderDeviceD3D12* device,
									DeviceContext* context,
									UINT hdrTextureGpuHeapIdx,
									std::shared_ptr<TextureD3D12> cubeRenderTarget,
									uint16 mapSize);

		// TODO: Make it suitable for rendering each frame
		// For now, this function should not be called every frame!
		void RenderIrradianceMap(DeviceContext* context,
								 UINT envCubeMapGpuHeapIdx,
								 std::shared_ptr<TextureD3D12> cubeRenderTarget,
								 uint16 mapSize);

		// TODO: Make it suitable for rendering each frame
		// For now, this function should not be called every frame!
		void RenderPrefilteredMap(DeviceContext* context,
								  UINT envCubeMapGpuHeapIdx,
								  std::shared_ptr<TextureD3D12> cubeRenderTarget,
								  uint16 mapSize);

		std::shared_ptr<TextureD3D12> GetBrdfLut() const { return m_BrdfLut; }

		static constexpr uint16 PREFILTERED_MAP_SIZE = 256;
		static constexpr uint16 PREFILTERED_MIP_LEVELS = 9;
		static constexpr DXGI_FORMAT HDR_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;

	private:
		void InitBRDF(RenderDeviceD3D12* device, DeviceContext* context);
		void RenderCubeMap(DeviceContext* context, PipelineState& pso, ShaderBinder* binder, TextureD3D12* texture, uint16 size, uint16 mipLevel = 0);
		void InitCube(RenderDeviceD3D12* device, DeviceContext* context);

	private:
		struct PassData
		{
			float Roughness;
			UINT EnvMapSize;
			UINT EnvMapLods;
			UINT TextureIdx;
		};

		static constexpr uint16 BRDF_LUT_SIZE = 512;

		std::shared_ptr<TextureD3D12> m_BrdfLut;

		PipelineState m_PsoHDR2Cube;
		PipelineState m_PsoGenIrrMap;
		PipelineState m_PsoGenPrefilteredMap;

		std::shared_ptr<ShaderBinder> m_PsoHDR2CubeBinder;
		std::shared_ptr<ShaderBinder> m_PsoGenIrrMapBinder;
		std::shared_ptr<ShaderBinder> m_PsoGenPrefilteredMapBinder;

		std::shared_ptr<DynamicUploadBuffer> m_FaceBuff;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuff;

		XMMATRIX m_CubeProj;
		XMMATRIX m_CubeView[6];

		std::shared_ptr<IndexBufferD3D12> m_CubeIB;
		std::shared_ptr<VertexBufferD3D12> m_CubeVB;
	};
}