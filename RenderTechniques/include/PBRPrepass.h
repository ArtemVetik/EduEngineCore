#pragma once
#include "framework.h"

#include <Camera.h>
#include <PipelineState.h>
#include <TextureD3D12.h>
#include <BufferD3D12.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RENDERTECHNIQUES_API PBRPrepass
	{
	public:
		PBRPrepass(RenderDeviceD3D12* device, DeviceContext* context, bool cpuHandles = true);

		void GenerateCubemapFromHDR(RenderDeviceD3D12* device,
									DeviceContext* context,
									std::shared_ptr<TextureD3D12> hdrTexture,
									std::shared_ptr<TextureD3D12> cubeRenderTarget,
									uint16 mapSize);

		void RenderIrradianceMap(DeviceContext* context,
								 std::shared_ptr<TextureD3D12> envCubeMap,
								 std::shared_ptr<TextureD3D12> cubeRenderTarget,
								 uint16 mapSize);

		void RenderPrefilteredMap(DeviceContext* context,
								  std::shared_ptr<TextureD3D12> envCubeMap,
								  std::shared_ptr<TextureD3D12> cubeRenderTarget,
								  uint16 mapSize);

		std::shared_ptr<TextureD3D12> GetBrdfLut() const { return m_BrdfLut; }

		static constexpr uint16 PREFILTERED_MAP_SIZE = 256;
		static constexpr uint16 PREFILTERED_MIP_LEVELS = 9;
		static constexpr DXGI_FORMAT HDR_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;

	private:
		void InitBRDF(RenderDeviceD3D12* device, DeviceContext* context, bool cpuHandles);
		void RenderCubeMap(DeviceContext* context, PipelineState& pso, ShaderBinder* binder, TextureD3D12* texture, uint16 size, uint16 mipLevel = 0);
		void InitCube(RenderDeviceD3D12* device, DeviceContext* context);

	private:
		static constexpr uint16 BRDF_LUT_SIZE = 512;

		std::shared_ptr<TextureD3D12> m_BrdfLut;

		PipelineState m_PsoHDR2Cube;
		PipelineState m_PsoGenIrrMap;
		PipelineState m_PsoGenPrefilteredMap;

		std::shared_ptr<ShaderBinder> m_PsoHDR2CubeBinder;
		std::shared_ptr<ShaderBinder> m_PsoGenIrrMapBinder;
		std::shared_ptr<ShaderBinder> m_PsoGenPrefilteredMapBinder;

		std::shared_ptr<DynamicUploadBuffer> m_PassBuffVS;
		std::shared_ptr<DynamicUploadBuffer> m_PassBuffPS;

		XMMATRIX m_CubeProj;
		XMMATRIX m_CubeView[6];

		std::shared_ptr<IndexBufferD3D12> m_CubeIB;
		std::shared_ptr<VertexBufferD3D12> m_CubeVB;
	};
}