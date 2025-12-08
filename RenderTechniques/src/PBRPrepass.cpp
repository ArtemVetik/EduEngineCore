#include "PBRPrepass.h"
#include "GenerateMipMaps.h"
#include "Asserts.h"

#include <stb_image.h>
#include <SimpleMath.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	PBRPrepass::PBRPrepass(RenderDeviceD3D12* device, DeviceContext* context) :
		m_SkyLod(0)
	{
		InitCube(device, context);
		InitTextures(device, context);
		InitSkyboxPSO(device);
	}

	void PBRPrepass::GenerateTextures(const char* hdrFileName, RenderDeviceD3D12* device, DeviceContext* context)
	{
		stbi_set_flip_vertically_on_load(true);
		int width, height, nrComponents;
		float* data = stbi_loadf(hdrFileName, &width, &height, &nrComponents, 3);
		unsigned int hdrTexture;

		if (!data)
		{
			ASSERT_FAILED("Failed to load HDR image: ", hdrFileName);
			return;
		}

		//
		// HDR Environment 2D Map
		//
		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.MipLevels = 1;
		texDesc.DepthOrArraySize = 1;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		auto HDREnvMap = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueMask::Direct);
		HDREnvMap->SetName(L"PBR_HDR_Env_Map");
		HDREnvMap->LoadData(context, data);
		HDREnvMap->CreateSRV(&srvDesc);

		stbi_image_free(data);

		CommandContext* cmdCtx = context->GetCommandCtx();

		cmdCtx->TransitionResource(HDREnvMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdCtx->TransitionResource(m_HDRCubeEnvMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmdCtx->TransitionResource(m_IrradianceMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmdCtx->TransitionResource(m_PrefilteredMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmdCtx->TransitionResource(m_BrdfLut.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmdCtx->FlushResourceBarriers();

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
		auto ps_GenBrdfLut = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\PBRPrepass.hlsl", L"PS_GenBrdfLut", L"ps_6_0", nullptr, sDesc);

		std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = false;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		PipelineState psoHDR2Cube;
		psoHDR2Cube.SetDepthStencilState(dss);
		psoHDR2Cube.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		psoHDR2Cube.SetShader(vs_Cube);
		psoHDR2Cube.SetShader(ps_HDR2Cube);
		psoHDR2Cube.SetRTVFormat(m_HDRCubeEnvMap->GetD3D12Resource()->GetDesc().Format);
		psoHDR2Cube.Build(device);

		std::shared_ptr<ShaderBinder> psoHDR2CubeBinder = psoHDR2Cube.CreateShaderBinder();

		PipelineState psoGenIrrMap;
		psoGenIrrMap.SetDepthStencilState(dss);
		psoGenIrrMap.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		psoGenIrrMap.SetShader(vs_Cube);
		psoGenIrrMap.SetShader(ps_GenIrradianceMap);
		psoGenIrrMap.SetRTVFormat(m_IrradianceMap->GetD3D12Resource()->GetDesc().Format);
		psoGenIrrMap.Build(device);

		std::shared_ptr<ShaderBinder> psoGenIrrMapBinder = psoGenIrrMap.CreateShaderBinder();

		PipelineState psoGenPrefilteredMap;
		psoGenPrefilteredMap.SetDepthStencilState(dss);
		psoGenPrefilteredMap.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		psoGenPrefilteredMap.SetShader(vs_Cube);
		psoGenPrefilteredMap.SetShader(ps_GenPrefilteredMap);
		psoGenPrefilteredMap.SetRTVFormat(m_PrefilteredMap->GetD3D12Resource()->GetDesc().Format);
		psoGenPrefilteredMap.Build(device);

		std::shared_ptr<ShaderBinder> psoGenPrefilteredMapBinder = psoGenPrefilteredMap.CreateShaderBinder();

		PipelineState psoGenBrdfLut;
		psoGenBrdfLut.SetDepthStencilState(dss);
		psoGenBrdfLut.SetShader(vs_Plane);
		psoGenBrdfLut.SetShader(ps_GenBrdfLut);
		psoGenBrdfLut.SetRTVFormat(m_BrdfLut->GetD3D12Resource()->GetDesc().Format);
		psoGenBrdfLut.Build(device);

		std::shared_ptr<ShaderBinder> psoGenBrdfLutBinder = psoGenBrdfLut.CreateShaderBinder();

		auto m_PassBuffVS = std::make_shared<DynamicUploadBuffer>(device, QueueMask::Direct);
		auto m_PassBuffPS = std::make_shared<DynamicUploadBuffer>(device, QueueMask::Direct);

		psoHDR2CubeBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffVS);
		psoHDR2CubeBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gEnvMap2D", HDREnvMap);

		psoGenIrrMapBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffVS);
		psoGenIrrMapBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gEnvCubeMap", m_HDRCubeEnvMap);

		psoGenPrefilteredMapBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffVS);
		psoGenPrefilteredMapBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gEnvCubeMap", m_HDRCubeEnvMap);
		psoGenPrefilteredMapBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffPS);

		//
		//	Render Pass
		//

		XMMATRIX projM = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 0.1f, 10.0f);

		XMMATRIX viewM[6]
		{
			XMMatrixLookToLH({0,0,0}, {1,0,0}, {0,1,0}),   // +X
			XMMatrixLookToLH({0,0,0}, {-1,0,0}, {0,1,0}),  // -X
			XMMatrixLookToLH({0,0,0}, {0,1,0}, {0,0,-1}),  // +Y
			XMMatrixLookToLH({0,0,0}, {0,-1,0}, {0,0,1}),  // -Y
			XMMatrixLookToLH({0,0,0}, {0,0,1}, {0,1,0}),   // +Z
			XMMatrixLookToLH({0,0,0}, {0,0,-1}, {0,1,0}),  // -Z
		};

		auto RenderCubeMap = [&](PipelineState& pso, ShaderBinder* binder, TextureD3D12* texture, uint16 size, uint16 mipLevel = 0)
			{
				for (uint8 depth = 0; depth < 6; depth++)
				{
					XMFLOAT4X4 MVP;
					DirectX::XMStoreFloat4x4(&MVP, DirectX::XMMatrixTranspose(viewM[depth] * projM));
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
			};

		// Generate HDR Environment CubeMap
		RenderCubeMap(psoHDR2Cube, psoHDR2CubeBinder.get(), m_HDRCubeEnvMap.get(), ENV_CUBEMAP_SIZE);

		GenerateMipMaps genMipMaps(device);
		genMipMaps.Generate(context, m_HDRCubeEnvMap);

		// Must complete barrier befote next passes, as m_HDRCubeEnvMap used in the next passes
		cmdCtx->TransitionResource(m_HDRCubeEnvMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		// Generate Irradiance Map
		RenderCubeMap(psoGenIrrMap, psoGenIrrMapBinder.get(), m_IrradianceMap.get(), IRRADIANCE_MAP_SIZE);

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
			passCB.EnvMapSize = PREFILTERED_MAP_SIZE;
			passCB.EnvMapLods = PREFILTERED_MIP_LEVELS;

			m_PassBuffPS->LoadData(context, passCB);

			// Generate Prefiltered Map (for each mip level)
			RenderCubeMap(psoGenPrefilteredMap, psoGenPrefilteredMapBinder.get(), m_PrefilteredMap.get(), PREFILTERED_MAP_SIZE / (1 << mip), mip);
		}

		//
		// Generate BRDF Lut
		//
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


		// Transition all resources for use in pixel shader
		cmdCtx->TransitionResource(m_IrradianceMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdCtx->TransitionResource(m_PrefilteredMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdCtx->TransitionResource(m_BrdfLut.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void PBRPrepass::RenderSky(RenderDeviceD3D12* device, DeviceContext* context, Camera* camera)
	{
		struct PassCB
		{
			XMFLOAT4X4 View;
			XMFLOAT4X4 Proj;
			float Lod;
			XMUINT3 Padding;
		};

		PassCB cb = {};
		XMStoreFloat4x4(&cb.View, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&camera->GetViewMatrix())));
		XMStoreFloat4x4(&cb.Proj, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&camera->GetProjectionMatrix())));
		cb.Lod = m_SkyLod;

		m_SkyboxPassBuff->LoadData(context, cb);

		m_PsoSkybox.CommitAll(context, m_PsoSkyboxBinder.get());

		context->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_CubeVB->GetView());
		context->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_CubeIB->GetView());

		context->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_CubeIB->GetLength(), 1, 0, 0, 0);
	}

	void PBRPrepass::SetSkyTex(std::shared_ptr<TextureD3D12> texture)
	{
		m_PsoSkyboxBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gCubeMap", texture);
	}

	void PBRPrepass::InitCube(RenderDeviceD3D12* device, DeviceContext* context)
	{
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

	void PBRPrepass::InitTextures(RenderDeviceD3D12* device, DeviceContext* context)
	{
		//
		// HDR Environment CubeMap
		//
		{
			// Calculation maximum mip level depending on ENV_CUBEMAP_SIZE
			DWORD mipLevels;
			_BitScanReverse(&mipLevels, ENV_CUBEMAP_SIZE);

			VERIFY_EXPR(mipLevels < UINT16_MAX, "");

			D3D12_RESOURCE_DESC texDesc = {};
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.DepthOrArraySize = 6;
			texDesc.Width = ENV_CUBEMAP_SIZE;
			texDesc.Height = ENV_CUBEMAP_SIZE;
			texDesc.MipLevels = mipLevels + 1;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = texDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Format = texDesc.Format;
			rtvDesc.Texture2DArray.ArraySize = 1;

			m_HDRCubeEnvMap = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueMask::Direct);
			m_HDRCubeEnvMap->SetName(L"PBR_HDR_Env_CubeMap");
			m_HDRCubeEnvMap->CreateRTV_Array(rtvDesc);
			m_HDRCubeEnvMap->CreateSRV(&srvDesc);
		}

		//
		// Irradiance Map
		//
		{
			D3D12_RESOURCE_DESC texDesc = {};
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.MipLevels = 1;
			texDesc.DepthOrArraySize = 6;
			texDesc.Width = IRRADIANCE_MAP_SIZE;
			texDesc.Height = IRRADIANCE_MAP_SIZE;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = texDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = 1;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Format = texDesc.Format;
			rtvDesc.Texture2DArray.ArraySize = 1;

			m_IrradianceMap = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueMask::Direct);
			m_IrradianceMap->SetName(L"PBR_Irradiance_Map");
			m_IrradianceMap->CreateRTV_Array(rtvDesc);
			m_IrradianceMap->CreateSRV(&srvDesc);
		}

		//
		// Prefiltered Map
		//
		{
			D3D12_RESOURCE_DESC texDesc = {};
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.MipLevels = PREFILTERED_MIP_LEVELS;
			texDesc.DepthOrArraySize = 6;
			texDesc.Width = PREFILTERED_MAP_SIZE;
			texDesc.Height = PREFILTERED_MAP_SIZE;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = texDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Format = texDesc.Format;
			rtvDesc.Texture2DArray.ArraySize = 1;

			m_PrefilteredMap = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueMask::Direct);
			m_PrefilteredMap->SetName(L"PBR_Prefiltered_Map");
			m_PrefilteredMap->CreateRTV_Array(rtvDesc);
			m_PrefilteredMap->CreateSRV(&srvDesc);
		}

		//
		// BRDF Lut
		//
		{
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

			m_BrdfLut = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueMask::Direct);
			m_BrdfLut->SetName(L"PBR_BRDF_Lut");
			m_BrdfLut->CreateRTV(&rtvDesc);
			m_BrdfLut->CreateSRV(&srvDesc);
		}
	}

	void PBRPrepass::InitSkyboxPSO(RenderDeviceD3D12* device)
	{
		ShaderResourceDesc resDesc[]
		{
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		sDesc.ResourceDesc = resDesc;
		sDesc.ResourceNum = _countof(resDesc);

		std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

		auto vs_Skybox = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Skybox.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);
		auto ps_Skybox = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Skybox.hlsl", L"PS", L"ps_6_0", nullptr, sDesc);

		m_PsoSkybox.SetDepthStencilState(dss);
		m_PsoSkybox.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
		m_PsoSkybox.SetShader(vs_Skybox);
		m_PsoSkybox.SetShader(ps_Skybox);
		m_PsoSkybox.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
		m_PsoSkybox.Build(device);
		m_PsoSkybox.SetName(L"PSO_Skybox");

		m_SkyboxPassBuff = std::make_shared<DynamicUploadBuffer>(device, QueueMask::Direct);

		m_PsoSkyboxBinder = m_PsoSkybox.CreateShaderBinder();
		m_PsoSkyboxBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_SkyboxPassBuff);
		m_PsoSkyboxBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_SkyboxPassBuff);
		m_PsoSkyboxBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "gCubeMap", m_HDRCubeEnvMap);
	}
}