#include "PBRPrepass.h"
#include "GenerateMipMaps.h"
#include "Asserts.h"

#include <SimpleMath.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	PBRPrepass::PBRPrepass(RenderDeviceD3D12* device, DeviceContext* context, bool cpuHandles)
	{
		m_PassBuffVS = std::make_shared<DynamicUploadBuffer>(device);
		m_PassBuffPS = std::make_shared<DynamicUploadBuffer>(device);

		InitCube(device, context);

		//
		//	Generate PSO
		//

		ShaderResourceDesc resDesc[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceDesc = resDesc;
		sDesc.ResourceNum = _countof(resDesc);

		auto vs_Cube = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBRPrepass.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto vs_Plane = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto ps_HDR2Cube = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBRPrepass.hlsl", L"PS_HDR2Cube", L"ps_6_0", nullptr, sDesc);
		auto ps_GenIrradianceMap = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBRPrepass.hlsl", L"PS_GenIrradianceMap", L"ps_6_0", nullptr, sDesc);
		auto ps_GenPrefilteredMap = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBRPrepass.hlsl", L"PS_GenPrefilteredMap", L"ps_6_0", nullptr, sDesc);

		std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = false;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		m_PsoHDR2Cube.SetDepthStencilState(dss);
		m_PsoHDR2Cube.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		m_PsoHDR2Cube.SetShader(vs_Cube);
		m_PsoHDR2Cube.SetShader(ps_HDR2Cube);
		m_PsoHDR2Cube.SetRTVFormat(HDR_FORMAT);
		m_PsoHDR2Cube.Build(device);

		m_PsoHDR2CubeBinder = m_PsoHDR2Cube.CreateShaderBinder();

		m_PsoGenIrrMap.SetDepthStencilState(dss);
		m_PsoGenIrrMap.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		m_PsoGenIrrMap.SetShader(vs_Cube);
		m_PsoGenIrrMap.SetShader(ps_GenIrradianceMap);
		m_PsoGenIrrMap.SetRTVFormat(HDR_FORMAT);
		m_PsoGenIrrMap.Build(device);

		m_PsoGenIrrMapBinder = m_PsoGenIrrMap.CreateShaderBinder();

		m_PsoGenPrefilteredMap.SetDepthStencilState(dss);
		m_PsoGenPrefilteredMap.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		m_PsoGenPrefilteredMap.SetShader(vs_Cube);
		m_PsoGenPrefilteredMap.SetShader(ps_GenPrefilteredMap);
		m_PsoGenPrefilteredMap.SetRTVFormat(HDR_FORMAT);
		m_PsoGenPrefilteredMap.Build(device);

		m_PsoGenPrefilteredMapBinder = m_PsoGenPrefilteredMap.CreateShaderBinder();

		m_PsoHDR2CubeBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffVS);
		m_PsoGenIrrMapBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffVS);
		m_PsoGenPrefilteredMapBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffVS);
		m_PsoGenPrefilteredMapBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffPS);

		InitBRDF(device, context, cpuHandles);
	}

	void PBRPrepass::GenerateCubemapFromHDR(RenderDeviceD3D12* device,
											DeviceContext* context,
											std::shared_ptr<TextureD3D12> hdrTexture,
											std::shared_ptr<TextureD3D12> cubeRenderTarget,
											uint16 mapSize)
	{
		m_PsoHDR2CubeBinder->DryMutableResources();
		m_PsoHDR2CubeBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gEnvMap2D", hdrTexture);

		CommandContext* cmdCtx = context->GetCommandCtx();

		// Generate HDR Environment CubeMap
		RenderCubeMap(context, m_PsoHDR2Cube, m_PsoHDR2CubeBinder.get(), cubeRenderTarget.get(), mapSize);

		GenerateMipMaps genMipMaps(device);
		genMipMaps.Generate(context, cubeRenderTarget);

		// Must complete barrier before next passes, as m_HDRCubeEnvMap used in the next passes
		cmdCtx->TransitionResource(cubeRenderTarget.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	}

	void PBRPrepass::RenderIrradianceMap(DeviceContext* context,
										 std::shared_ptr<TextureD3D12> envCubeMap,
										 std::shared_ptr<TextureD3D12> cubeRenderTarget,
										 uint16 mapSize)
	{
		m_PsoGenIrrMapBinder->DryMutableResources();
		m_PsoGenIrrMapBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gEnvCubeMap", envCubeMap);
		RenderCubeMap(context, m_PsoGenIrrMap, m_PsoGenIrrMapBinder.get(), cubeRenderTarget.get(), mapSize);

		context->GetCommandCtx()->TransitionResource(cubeRenderTarget.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void PBRPrepass::RenderPrefilteredMap(DeviceContext* context,
										  std::shared_ptr<TextureD3D12> envCubeMap,
										  std::shared_ptr<TextureD3D12> cubeRenderTarget,
										  uint16 mapSize)
	{
		m_PsoGenPrefilteredMapBinder->DryMutableResources();
		m_PsoGenPrefilteredMapBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gEnvCubeMap", envCubeMap);

		for (uint16 mip = 0; mip < PREFILTERED_MIP_LEVELS; mip++)
		{
			struct CB
			{
				float Roughness;
				UINT EnvMapSize;
				UINT EnvMapLods;
				UINT Padding;
			};

			CB passCB = {};
			passCB.Roughness = static_cast<float>(mip) / static_cast<float>(PREFILTERED_MIP_LEVELS - 1);
			passCB.EnvMapSize = mapSize;
			passCB.EnvMapLods = PREFILTERED_MIP_LEVELS;

			m_PassBuffPS->LoadData(context, passCB);

			// Generate Prefiltered Map (for each mip level)
			RenderCubeMap(context, m_PsoGenPrefilteredMap, m_PsoGenPrefilteredMapBinder.get(), cubeRenderTarget.get(), mapSize / (1 << mip), mip);
		}

		context->GetCommandCtx()->TransitionResource(cubeRenderTarget.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void PBRPrepass::InitBRDF(RenderDeviceD3D12* device, DeviceContext* context, bool cpuHandles)
	{
		//
		// BRDF Lut
		//
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.Width = BRDF_LUT_SIZE;
		texDesc.Height = BRDF_LUT_SIZE;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Format = DXGI_FORMAT_R16G16_FLOAT;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Format = texDesc.Format;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		m_BrdfLut = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
		m_BrdfLut->SetName(L"PBR_BRDF_Lut");
		m_BrdfLut->CreateRTV(&rtvDesc);
		m_BrdfLut->CreateSRV(&srvDesc, cpuHandles);

		//
		// Init BRFG Pso
		//
		ShaderResourceDesc resDesc[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceDesc = resDesc;
		sDesc.ResourceNum = _countof(resDesc);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = false;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		auto vs_Plane = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto ps_GenBrdfLut = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBRPrepass.hlsl", L"PS_GenBrdfLut", L"ps_6_0", nullptr, sDesc);

		PipelineState psoGenBrdfLut;
		psoGenBrdfLut.SetDepthStencilState(dss);
		psoGenBrdfLut.SetShader(vs_Plane);
		psoGenBrdfLut.SetShader(ps_GenBrdfLut);
		psoGenBrdfLut.SetRTVFormat(m_BrdfLut->GetD3D12Resource()->GetDesc().Format);
		psoGenBrdfLut.Build(device);

		auto psoGenBrdfLutBinder = psoGenBrdfLut.CreateShaderBinder();

		//
		// Generate BRDF Lut
		//
		CommandContext* cmdCtx = context->GetCommandCtx();
		cmdCtx->TransitionResource(m_BrdfLut.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		auto handle = m_BrdfLut->GetRTVView()->GetCpuHandle();
		cmdCtx->SetRenderTargets(1, &handle, true, nullptr);

		D3D12_VIEWPORT vp = {};
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		vp.Width = BRDF_LUT_SIZE;
		vp.Height = BRDF_LUT_SIZE;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		D3D12_RECT scissor = { 0, 0, BRDF_LUT_SIZE, BRDF_LUT_SIZE };
		cmdCtx->SetViewports(&vp, 1);
		cmdCtx->SetScissorRects(&scissor, 1);

		psoGenBrdfLut.CommitAll(context, psoGenBrdfLutBinder.get());

		cmdCtx->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmdCtx->GetCmdList()->DrawInstanced(3, 1, 0, 0);

		cmdCtx->TransitionResource(m_BrdfLut.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void PBRPrepass::RenderCubeMap(DeviceContext* context, PipelineState& pso, ShaderBinder* binder, TextureD3D12* texture, uint16 size, uint16 mipLevel)
	{
		CommandContext* cmdCtx = context->GetCommandCtx();

		for (uint8 depth = 0; depth < 6; depth++)
		{
			XMFLOAT4X4 MVP;
			DirectX::XMStoreFloat4x4(&MVP, DirectX::XMMatrixTranspose(m_CubeView[depth] * m_CubeProj));
			m_PassBuffVS->LoadData(context, MVP);

			auto handle = texture->GetRTVView()->GetCpuHandle(mipLevel * 6 + depth);
			cmdCtx->SetRenderTargets(1, &handle, true, nullptr);

			D3D12_VIEWPORT vp = {};
			vp.TopLeftX = 0;
			vp.TopLeftY = 0;
			vp.Width = size;
			vp.Height = size;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;

			D3D12_RECT scissor = { 0, 0, size, size };
			cmdCtx->SetViewports(&vp, 1);
			cmdCtx->SetScissorRects(&scissor, 1);

			pso.CommitAll(context, binder);

			cmdCtx->GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdCtx->GetCmdList()->IASetVertexBuffers(0, 1, &m_CubeVB->GetView());
			cmdCtx->GetCmdList()->IASetIndexBuffer(&m_CubeIB->GetView());

			cmdCtx->GetCmdList()->DrawIndexedInstanced(m_CubeIB->GetLength(), 1, 0, 0, 0);
		}
	}

	void PBRPrepass::InitCube(RenderDeviceD3D12* device, DeviceContext* context)
	{
		m_CubeProj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);

		m_CubeView[0] = XMMatrixLookToLH({ 0,0,0 }, { 1,0,0 }, { 0,1,0 });  // +X
		m_CubeView[1] = XMMatrixLookToLH({ 0,0,0 }, { -1,0,0 }, { 0,1,0 }); // -X
		m_CubeView[2] = XMMatrixLookToLH({ 0,0,0 }, { 0,1,0 }, { 0,0,-1 }); // +Y
		m_CubeView[3] = XMMatrixLookToLH({ 0,0,0 }, { 0,-1,0 }, { 0,0,1 }); // -Y
		m_CubeView[4] = XMMatrixLookToLH({ 0,0,0 }, { 0,0,1 }, { 0,1,0 });  // +Z
		m_CubeView[5] = XMMatrixLookToLH({ 0,0,0 }, { 0,0,-1 }, { 0,1,0 }); // -Z

		DirectX::XMFLOAT3 vertices[] =
		{
			// +X
			{ +1, +1, -1 }, { +1, -1, -1 }, { +1, -1, +1 }, { +1, +1, +1 },
			// -X
			{ -1, +1, +1 }, { -1, -1, +1 }, { -1, -1, -1 }, { -1, +1, -1 },
			// +Y
			{ -1, +1, +1 }, { -1, +1, -1 }, { +1, +1, -1 }, { +1, +1, +1 },
			// -Y
			{ -1, -1, -1 }, { -1, -1, +1 }, { +1, -1, +1 }, { +1, -1, -1 },
			// +Z
			{ -1, +1, +1 }, { +1, +1, +1 }, { +1, -1, +1 }, { -1, -1, +1 },
			// -Z
			{ +1, +1, -1 }, { -1, +1, -1 }, { -1, -1, -1 }, { +1, -1, -1 },
		};

		uint16 indices[] = {
			0,1,2,		0,2,3,		// +X
			4,5,6,		4,6,7,      // -X
			8,9,10,		8,10,11,	// +Y
			12,13,14,	12,14,15,	// -Y
			16,17,18,	16,18,19,	// +Z
			20,21,22,	20,22,23,	// -Z
		};

		m_CubeVB = std::make_shared<VertexBufferD3D12>(device, context, vertices, sizeof(DirectX::XMFLOAT3), _countof(vertices));
		m_CubeIB = std::make_shared<IndexBufferD3D12>(device, context, indices, sizeof(uint16), _countof(indices), DXGI_FORMAT_R16_UINT);

		m_CubeVB->SetName(L"VB Cube");
		m_CubeIB->SetName(L"IB Cube");
	}
}