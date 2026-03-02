#include "Skybox.h"

#include <stb_image.h>

namespace EduEngine
{
	Skybox::Skybox(const char* hdrFileName, RenderDeviceD3D12* device, DeviceContext* context, IBLRendering* IBLRendering) :
		m_Device(device),
		m_CpuTextureHandles(false),
		m_SkyLod(0)
	{
		//	TODO: Share cube buffers!
		// 
		//	Generate cube buffers
		//
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

		m_CubeVB = std::make_shared<VertexBufferD3D12>(m_Device, context, vertices, sizeof(DirectX::XMFLOAT3), _countof(vertices));
		m_CubeIB = std::make_shared<IndexBufferD3D12>(m_Device, context, indices, sizeof(uint16), _countof(indices), DXGI_FORMAT_R16_UINT);

		m_CubeVB->SetName(L"VB Cube");
		m_CubeIB->SetName(L"IB Cube");

		//
		// Init Textures
		//

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
			texDesc.Format = IBLRendering::HDR_FORMAT;
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

			m_HDRCubeEnvMap = std::make_shared<TextureD3D12>(m_Device, texDesc, nullptr, QueueId::Direct);
			m_HDRCubeEnvMap->SetName(L"PBR_HDR_Env_CubeMap");
			m_HDRCubeEnvMap->CreateRTV_Array(rtvDesc);
			m_HDRCubeEnvMap->CreateSRV(&srvDesc, m_CpuTextureHandles);
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
			texDesc.Format = IBLRendering::HDR_FORMAT;
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

			m_IrradianceMap = std::make_shared<TextureD3D12>(m_Device, texDesc, nullptr, QueueId::Direct);
			m_IrradianceMap->SetName(L"PBR_Irradiance_Map");
			m_IrradianceMap->CreateRTV_Array(rtvDesc);
			m_IrradianceMap->CreateSRV(&srvDesc, m_CpuTextureHandles);
		}

		//
		// Prefiltered Map
		//
		{
			D3D12_RESOURCE_DESC texDesc = {};
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.MipLevels = IBLRendering::PREFILTERED_MIP_LEVELS;
			texDesc.DepthOrArraySize = 6;
			texDesc.Width = IBLRendering::PREFILTERED_MAP_SIZE;
			texDesc.Height = IBLRendering::PREFILTERED_MAP_SIZE;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Format = IBLRendering::HDR_FORMAT;
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

			m_PrefilteredMap = std::make_shared<TextureD3D12>(m_Device, texDesc, nullptr, QueueId::Direct);
			m_PrefilteredMap->SetName(L"PBR_Prefiltered_Map");
			m_PrefilteredMap->CreateRTV_Array(rtvDesc);
			m_PrefilteredMap->CreateSRV(&srvDesc, m_CpuTextureHandles);
		}

		RebuildSky(hdrFileName, context, IBLRendering);

		//
		// Build Skybox PSO
		//
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

		LPCWSTR macros[]
		{
			L"HDR_OUTPUT", L"1",
			NULL, NULL
		};

		auto vs_Skybox = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Skybox.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto ps_SkyboxLDR = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Skybox.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);
		auto ps_SkyboxHDR = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Skybox.hlsl", L"PS", L"ps_6_6", macros, sDesc);

		m_SkyboxPassBuff = std::make_shared<DynamicUploadBuffer>(device);

		for (uint32 i = 0; i < 2; i++)
		{
			bool ldr = i == 0;

			m_PsoSkybox[i].SetDepthStencilState(dss);
			m_PsoSkybox[i].SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_PsoSkybox[i].SetShader(vs_Skybox);

			if (ldr)
			{
				m_PsoSkybox[i].SetShader(ps_SkyboxLDR);
				m_PsoSkybox[i].SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
			}
			else
			{
				m_PsoSkybox[i].SetShader(ps_SkyboxHDR);
				m_PsoSkybox[i].SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
			}

			m_PsoSkybox[i].Build(device);

			if (ldr)
				m_PsoSkybox[i].SetName(L"PSO_Skybox_LDR");
			else
				m_PsoSkybox[i].SetName(L"PSO_Skybox_HDR");

			m_PsoSkyboxBinder[i] = m_PsoSkybox[i].CreateShaderBinder();
			m_PsoSkyboxBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_SkyboxPassBuff);
			m_PsoSkyboxBinder[i]->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_SkyboxPassBuff);
		}

		m_SkyTextureGpuHeapIdx = m_HDRCubeEnvMap->GetSRVView()->GetGpuHeapIndex();
	}

	void Skybox::Render(DeviceContext* context, Camera* camera, bool hdr)
	{
		struct PassCB
		{
			XMFLOAT4X4 View;
			XMFLOAT4X4 Proj;
			float Lod;
			UINT TextureIdx;
			XMUINT2 Padding;
		};

		PassCB cb = {};
		XMStoreFloat4x4(&cb.View, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&camera->GetViewMatrix())));
		XMStoreFloat4x4(&cb.Proj, DirectX::XMMatrixTranspose(XMLoadFloat4x4(&camera->GetProjectionMatrix())));
		cb.Lod = m_SkyLod;
		cb.TextureIdx = m_SkyTextureGpuHeapIdx;

		m_SkyboxPassBuff->LoadData(context, cb);

		if (hdr)
			m_PsoSkybox[1].CommitAll(context, m_PsoSkyboxBinder[0].get());
		else
			m_PsoSkybox[0].CommitAll(context, m_PsoSkyboxBinder[0].get());

		context->GetCommandCtx()->GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->GetCommandCtx()->GetCmdList()->IASetVertexBuffers(0, 1, &m_CubeVB->GetView());
		context->GetCommandCtx()->GetCmdList()->IASetIndexBuffer(&m_CubeIB->GetView());

		context->GetCommandCtx()->GetCmdList()->DrawIndexedInstanced(m_CubeIB->GetLength(), 1, 0, 0, 0);
	}

	void Skybox::RebuildSky(const char* hdrFileName, DeviceContext* context, IBLRendering* IBLRendering)
	{
		//
		//	Load HDR Environment 2D Map
		//
		stbi_set_flip_vertically_on_load(true);
		int width, height, nrComponents;
		float* data = stbi_loadf(hdrFileName, &width, &height, &nrComponents, 3);
		unsigned int hdrTexture;

		if (!data)
		{
			ASSERT_FAILED("Failed to load HDR image: ", hdrFileName);
			return;
		}

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

		auto HDREnvMap = std::make_shared<TextureD3D12>(m_Device, texDesc, nullptr, QueueId::Direct);
		HDREnvMap->SetName(L"PBR_HDR_Env_Map");
		HDREnvMap->LoadData(context, data);
		HDREnvMap->CreateSRV(&srvDesc, false);

		stbi_image_free(data);

		context->GetCommandCtx()->TransitionResource(m_HDRCubeEnvMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		context->GetCommandCtx()->TransitionResource(m_IrradianceMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		context->GetCommandCtx()->TransitionResource(m_PrefilteredMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		IBLRendering->GenerateCubemapFromHDR(m_Device, context, HDREnvMap->GetSRVView()->GetGpuHeapIndex(), m_HDRCubeEnvMap, ENV_CUBEMAP_SIZE);
		IBLRendering->RenderIrradianceMap(context, m_HDRCubeEnvMap->GetSRVView()->GetGpuHeapIndex(), m_IrradianceMap, IRRADIANCE_MAP_SIZE);
		IBLRendering->RenderPrefilteredMap(context, m_HDRCubeEnvMap->GetSRVView()->GetGpuHeapIndex(), m_PrefilteredMap, IBLRendering::PREFILTERED_MAP_SIZE);

		context->GetCommandCtx()->TransitionResource(m_HDRCubeEnvMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context->GetCommandCtx()->TransitionResource(m_IrradianceMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		context->GetCommandCtx()->TransitionResource(m_PrefilteredMap.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
	}

	void Skybox::SetSky(UINT skyTextureGpuHeapIdx)
	{
		m_SkyTextureGpuHeapIdx = skyTextureGpuHeapIdx;
	}
}