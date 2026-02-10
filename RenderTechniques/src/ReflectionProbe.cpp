#include "ReflectionProbe.h"

#include <Asserts.h>

namespace EduEngine
{
	ReflectionProbe::ReflectionProbe(RenderDeviceD3D12* device, DeviceContext* context, Settings settings) :
		m_Settings(settings)
	{
		m_Center = { 0, 15.0f, 0 };
		m_Extents = { 10, 10, 10 };

		InitializeTextures(device, context);

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;
		sDesc.ResourceNum = 0;

		auto VS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ReflectionProbe.hlsl", L"VS", L"vs_6_6", nullptr, sDesc);
		auto PS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ReflectionProbe.hlsl", L"PS", L"ps_6_6", nullptr, sDesc);

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

		D3D12_INPUT_ELEMENT_DESC inputLayout[]
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL",	  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,	 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		m_PSO.SetDepthStencilState(dss);
		m_PSO.SetInputLayout({ inputLayout, _countof(inputLayout) });
		m_PSO.SetShader(VS);
		m_PSO.SetShader(PS);
		m_PSO.SetRTVFormat(DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_PSO.Build(device);

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(device);
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(device);

		m_Binder = m_PSO.CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbObject", m_ObjBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbObject", m_ObjBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);

		CSMRendering::Settings csmSettings = {};
		csmSettings.CascadesCount = 1;
		csmSettings.CSMSplits[0] = 1;

		m_CSMRendering = std::make_unique<CSMRendering>(device, context, csmSettings);
	}

	void ReflectionProbe::Bake(DeviceContext* context,
							   IBLRendering* iblRendering,
							   Skybox* skybox,
							   Light* lights,
							   uint32 numLights,
							   RenderObject* renderObjects,
							   uint32 objectsNum)
	{
		float nearValue = 0.1f;
		float farValue = 100.0f;

		Camera faceCamera[6]
		{
			Camera(m_Settings.TextureSize, m_Settings.TextureSize, XM_PIDIV2, nearValue, farValue, m_Center, { 1, 0, 0 }, { 0, 1, 0 }),
			Camera(m_Settings.TextureSize, m_Settings.TextureSize, XM_PIDIV2, nearValue, farValue, m_Center, {-1, 0, 0 }, { 0, 1, 0 }),
			Camera(m_Settings.TextureSize, m_Settings.TextureSize, XM_PIDIV2, nearValue, farValue, m_Center, { 0, 1, 0 }, { 0, 0,-1 }),
			Camera(m_Settings.TextureSize, m_Settings.TextureSize, XM_PIDIV2, nearValue, farValue, m_Center, { 0,-1, 0 }, { 0, 0, 1 }),
			Camera(m_Settings.TextureSize, m_Settings.TextureSize, XM_PIDIV2, nearValue, farValue, m_Center, { 0, 0, 1 }, { 0, 1, 0 }),
			Camera(m_Settings.TextureSize, m_Settings.TextureSize, XM_PIDIV2, nearValue, farValue, m_Center, { 0, 0,-1 }, { 0, 1, 0 }),
		};

		XMMATRIX proj = XMLoadFloat4x4(&faceCamera[0].GetProjectionMatrix());

		D3D12_VIEWPORT viewport = {};
		viewport.Width = m_Settings.TextureSize;
		viewport.Height = m_Settings.TextureSize;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0;
		viewport.MaxDepth = 1;

		D3D12_RECT scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

		CommandContext* commandContext = context->GetCommandCtx();

		for (size_t i = 0; i < 6; i++)
		{
			context->GetCommandCtx()->FlushResourceBarriers();

			m_CSMRendering->Update(context, &faceCamera[i], &lights[0]); // TODO: lights
			m_CSMRendering->Render(context, renderObjects, objectsNum);

			commandContext->SetViewports(&viewport, 1);
			commandContext->SetScissorRects(&scissorRect, 1);

			commandContext->SetRenderTargets(1, &m_ReflectionCube->GetRTVView()->GetCpuHandle(i), false, &m_DepthBuffer->GetDSVView()->GetCpuHandle());

			commandContext->GetCmdList()->ClearDepthStencilView(m_DepthBuffer->GetDSVView()->GetCpuHandle(),
				D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

			struct PassData
			{
				XMFLOAT4X4 ViewProj;
				XMFLOAT4X4 ShadowTransform;
				UINT ShadowmapIdx;
				XMUINT3 Padding1;
			} passData;

			XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(faceCamera[i].GetViewProjMatrix()));
			XMStoreFloat4x4(&passData.ShadowTransform, XMMatrixTranspose(m_CSMRendering->GetCascadeTransform(0)));
			passData.ShadowmapIdx = m_CSMRendering->GetSrv(0)->GetGpuHeapIndex();

			m_PassBuffer->LoadData(context, passData);
			m_PSO.CommitPso(context);

			for (uint32 obj = 0; obj < objectsNum; obj++)
			{
				for (uint32 mIdx = 0; mIdx < renderObjects[obj].Mesh->GetMeshCount(); mIdx++)
				{
					struct ObjData
					{
						XMFLOAT4X4 World;
						UINT AlbedoTexIdx;
						XMUINT3 Padding;
					} objData;

					XMStoreFloat4x4(&objData.World, XMMatrixTranspose(renderObjects[obj].World));
					objData.AlbedoTexIdx = renderObjects[obj].Mesh->GetTexture(mIdx)->GetD3D12Texture()->GetSRVView()->GetGpuHeapIndex();

					m_ObjBuffer->LoadData(context, objData);

					m_PSO.CommitBinder(context, m_Binder.get());
					commandContext->GetCmdList()->IASetIndexBuffer(&renderObjects[obj].Mesh->GetIndexBuffer(mIdx)->GetView());
					commandContext->GetCmdList()->IASetVertexBuffers(0, 1, &renderObjects[obj].Mesh->GetVertexBuffer(mIdx)->GetView());
					commandContext->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandContext->GetCmdList()->DrawIndexedInstanced(renderObjects[obj].Mesh->GetIndexCount(mIdx), 1, 0, 0, 0);
				}
			}

			skybox->Render(context, &faceCamera[i], true);
		}

		if (m_Settings.Flags & Flags::CREATE_IRRADIANCE_MAP)
			iblRendering->RenderIrradianceMap(context, m_ReflectionCube->GetSRVView()->GetGpuHeapIndex(), m_IrradianceMap, IRRADIANCE_MAP_SIZE);

		if (m_Settings.Flags & Flags::CREATE_PREFILTERED_MAP)
			iblRendering->RenderPrefilteredMap(context, m_ReflectionCube->GetSRVView()->GetGpuHeapIndex(), m_PrefilteredMap, IBLRendering::PREFILTERED_MAP_SIZE);
	}

	void ReflectionProbe::InitializeTextures(RenderDeviceD3D12* device, DeviceContext* context)
	{
		bool cpuTextureHandles = false;

		//
		// Reflection Cube
		//
		{
			D3D12_RESOURCE_DESC texDesc;
			ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Width = m_Settings.TextureSize;
			texDesc.Height = m_Settings.TextureSize;
			texDesc.Alignment = 0;
			texDesc.DepthOrArraySize = 6;
			texDesc.MipLevels = 1;
			texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			D3D12_CLEAR_VALUE optClear = {};
			optClear.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			optClear.DepthStencil.Depth = 0.0f;
			optClear.DepthStencil.Stencil = 0;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			rtvDesc.Texture2DArray.ArraySize = 1;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = texDesc.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MostDetailedMip = 0;
			srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
			srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

			m_ReflectionCube = std::make_shared<TextureD3D12>(device, texDesc, &optClear, QueueId::Direct);
			m_ReflectionCube->CreateRTV_Array(rtvDesc);
			m_ReflectionCube->CreateSRV(&srvDesc, cpuTextureHandles);
			m_ReflectionCube->SetName(L"m_ReflectionCube");

			context->GetCommandCtx()->TransitionResource(m_ReflectionCube.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		//
		// Depth Buffer
		//
		{
			D3D12_RESOURCE_DESC texDesc;
			ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Width = m_Settings.TextureSize;
			texDesc.Height = m_Settings.TextureSize;
			texDesc.Alignment = 0;
			texDesc.DepthOrArraySize = 6;
			texDesc.MipLevels = 1;
			texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.Texture2D.MipSlice = 0;

			D3D12_CLEAR_VALUE optClear = {};
			optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			optClear.DepthStencil.Depth = 0.0f;
			optClear.DepthStencil.Stencil = 0;

			m_DepthBuffer = std::make_shared<TextureD3D12>(device, texDesc, &optClear, QueueId::Direct);
			m_DepthBuffer->CreateDSV(&dsvDesc);

			context->GetCommandCtx()->TransitionResource(m_DepthBuffer.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		//
		// Irradiance Map
		//
		if (m_Settings.Flags & Flags::CREATE_IRRADIANCE_MAP)
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

			m_IrradianceMap = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
			m_IrradianceMap->SetName(L"Reflection_Irradiance_Map");
			m_IrradianceMap->CreateRTV_Array(rtvDesc);
			m_IrradianceMap->CreateSRV(&srvDesc, cpuTextureHandles);

			context->GetCommandCtx()->TransitionResource(m_IrradianceMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		//
		// Prefiltered Map
		//
		if (m_Settings.Flags & Flags::CREATE_PREFILTERED_MAP)
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

			m_PrefilteredMap = std::make_shared<TextureD3D12>(device, texDesc, nullptr, QueueId::Direct);
			m_PrefilteredMap->SetName(L"Reflection_Prefiltered_Map");
			m_PrefilteredMap->CreateRTV_Array(rtvDesc);
			m_PrefilteredMap->CreateSRV(&srvDesc, cpuTextureHandles);

			context->GetCommandCtx()->TransitionResource(m_PrefilteredMap.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		}

		context->GetCommandCtx()->FlushResourceBarriers();
	}

	ReflectionProbesManager::ReflectionProbesManager(RenderDeviceD3D12* device, DeviceContext* context) :
		m_Device(device),
		m_Count(0)
	{
		m_ReflectionProbes.reserve(MAX_REFLECTION_PROBES);

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		buffDesc.Alignment = 0;
		buffDesc.Height = 1;
		buffDesc.Width = sizeof(ReflectionProbesData) * MAX_REFLECTION_PROBES;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.Format = DXGI_FORMAT_UNKNOWN;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = MAX_REFLECTION_PROBES;
		srvDesc.Buffer.StructureByteStride = sizeof(ReflectionProbesData);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		m_GpuBuffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);
		m_GpuBuffer->CreateSRV(&srvDesc, false);
	}

	ReflectionProbesManager::~ReflectionProbesManager()
	{
		m_ReflectionProbes.clear();
	}

	ReflectionProbe* ReflectionProbesManager::Add(DeviceContext* context, ReflectionProbe::Settings settings, XMFLOAT3 position, XMFLOAT3 extents)
	{
		if (m_Count >= MAX_REFLECTION_PROBES)
		{
			LOG_ERROR("Max reflection probes num exceeded (", MAX_REFLECTION_PROBES, ")");
			return nullptr;
		}

		auto newProbe = std::make_unique<ReflectionProbe>(m_Device, context, settings);
		newProbe->SetCenter(position);
		newProbe->SetExtents(extents);

		m_ReflectionProbes.emplace_back(std::move(newProbe));
		m_Count++;

		return m_ReflectionProbes.back().get();
	}

	void ReflectionProbesManager::RemoveAt(uint32 index)
	{
		if (m_Count == 0)
			return;

		m_ReflectionProbes.erase(m_ReflectionProbes.begin() + index);
		m_Count--;
	}

	void ReflectionProbesManager::RebuildBuffer(DeviceContext* context)
	{
		if (m_Count == 0)
			return;

		ReflectionProbesData data[MAX_REFLECTION_PROBES];

		for (uint32 i = 0; i < m_Count; i++)
		{
			data[i].Position = m_ReflectionProbes[i]->GetCenter();
			data[i].BoxExtents = m_ReflectionProbes[i]->GetExtents();
			data[i].IrradianceMapIdx = m_ReflectionProbes[i]->GetIrradianceMap()->GetSRVView()->GetGpuHeapIndex();
			data[i].PrefilteredMapIdx = m_ReflectionProbes[i]->GetPrefilteredMap()->GetSRVView()->GetGpuHeapIndex();
		}

		UINT byteSize = sizeof(ReflectionProbesData) * m_Count;
		m_GpuBuffer->LoadData(context, data, &byteSize);
	}
}