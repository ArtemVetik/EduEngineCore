#include "VirtualShadowMap.h"

#include <ShaderCache.h>
#include <SimpleMath.h>

using namespace DirectX;

namespace EduEngine
{
	namespace VSM
	{
		struct ResView
		{
			bool Use = false;
			DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
		};

		void CreateTexture(RenderDeviceD3D12* device,
			std::shared_ptr<TextureD3D12>& texture,
			uint32 width,
			uint32 height,
			DXGI_FORMAT format,
			D3D12_RESOURCE_FLAGS flags,
			ResView clear,
			ResView srv,
			ResView dsv,
			ResView uav,
			const wchar_t* name)
		{
			D3D12_RESOURCE_DESC buffDesc = {};
			buffDesc.Width = width;
			buffDesc.Height = height;
			buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			buffDesc.Alignment = 0;
			buffDesc.DepthOrArraySize = 1;
			buffDesc.MipLevels = 1;
			buffDesc.SampleDesc.Count = 1;
			buffDesc.SampleDesc.Quality = 0;
			buffDesc.Format = format;
			buffDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			buffDesc.Flags = flags;

			D3D12_CLEAR_VALUE optClear = {};
			optClear.Format = clear.Format;
			optClear.DepthStencil.Depth = 0.0f;
			optClear.DepthStencil.Stencil = 0;

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Format = dsv.Format;
			dsvDesc.Texture2D.MipSlice = 0;

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = srv.Format;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			srvDesc.Texture2D.PlaneSlice = 0;

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = uav.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = 0;
			uavDesc.Texture2D.PlaneSlice = 0;

			texture = std::make_shared<TextureD3D12>(device, buffDesc, clear.Use ? &optClear : nullptr, QueueId::Direct);
			if (srv.Use)
				texture->CreateSRV(&srvDesc, false);
			if (dsv.Use)
				texture->CreateDSV(&dsvDesc);
			if (uav.Use)
				texture->CreateUAV(&uavDesc, false);

			texture->SetName(name);
		}

		void CreateBuffer(RenderDeviceD3D12* device,
			DeviceContext* context,
			std::shared_ptr<BufferD3D12>& buffer,
			uint32 numElements,
			uint32 byteStride,
			DXGI_FORMAT format,
			D3D12_RESOURCE_FLAGS flags,
			ResView uav,
			const wchar_t* name)
		{
			D3D12_RESOURCE_DESC buffDesc = {};
			buffDesc.Width = numElements * byteStride;
			buffDesc.Height = 1;
			buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			buffDesc.Alignment = 0;
			buffDesc.DepthOrArraySize = 1;
			buffDesc.MipLevels = 1;
			buffDesc.SampleDesc.Count = 1;
			buffDesc.SampleDesc.Quality = 0;
			buffDesc.Format = format;
			buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			buffDesc.Flags = flags;

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = uav.Format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.NumElements = numElements;
			uavDesc.Buffer.StructureByteStride = byteStride;
			uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

			buffer = std::make_shared<BufferD3D12>(device, context, buffDesc, QueueId::Direct);

			if (uav.Use)
				buffer->CreateUAV(&uavDesc, false);

			buffer->SetName(name);
		}
	}

	VirtualShadowMap::VirtualShadowMap(RenderDeviceD3D12* device, DeviceContext* context) :
		m_Frame(0),
		m_DebugMode(VSMDebugMode::None)
	{
		VSM::CreateBuffer(device, context, m_ConstantsBuffer, 1, sizeof(CbBuffers), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE,
			{ false, DXGI_FORMAT_UNKNOWN }, L"m_ConstantsBuffer");

		struct PhysMetaEntry
		{
			UINT Flags;
			UINT LastUsedFrame;
			UINT AllocFrame;
			XMUINT2 PageAddr;
			UINT LevelIndex;
		};

		struct RenderListEntry
		{
			UINT Slot;
		};

		VSM::CreateBuffer(device, context, m_ClipmapBuffer, VSM_NUM_CLIPMAPS, sizeof(ClipmapLevelGPU), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_ClipmapBuffer");

		VSM::CreateTexture(device, m_PhysPool, VSM_POOL_SIZE * VSM_PAGE_SIZE, VSM_POOL_SIZE * VSM_PAGE_SIZE, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ false, DXGI_FORMAT_UNKNOWN },	// clear
			{ false, DXGI_FORMAT_UNKNOWN },	// srv
			{ false, DXGI_FORMAT_UNKNOWN },	// dsv
			{ true, DXGI_FORMAT_R32_UINT },	// uav
			L"m_PhysPool");

		VSM::CreateBuffer(device, context, m_PhysMeta, VSM_MAX_PHYS_PAGES, sizeof(PhysMetaEntry), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_PhysMeta");

		VSM::CreateTexture(device, m_PageRequest, VSM_PAGES_XY, VSM_PAGES_XY * VSM_NUM_CLIPMAPS, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ false, DXGI_FORMAT_UNKNOWN },	// clear
			{ false, DXGI_FORMAT_UNKNOWN }, // srv
			{ false, DXGI_FORMAT_UNKNOWN },	// dsv
			{ true, DXGI_FORMAT_R32_UINT },	// uav
			L"m_PageRequest");

		VSM::CreateTexture(device, m_PageTable, VSM_PAGES_XY, VSM_PAGES_XY * VSM_NUM_CLIPMAPS, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ false, DXGI_FORMAT_UNKNOWN },	// clear
			{ false, DXGI_FORMAT_UNKNOWN }, // srv
			{ false, DXGI_FORMAT_UNKNOWN },	// dsv
			{ true, DXGI_FORMAT_R32_UINT },	// uav
			L"m_PageTable");

		VSM::CreateBuffer(device, context, m_PageLists[0], 4 * (VSM_MAX_PHYS_PAGES + 1), sizeof(UINT), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_PageLists[0]");

		VSM::CreateBuffer(device, context, m_PageLists[1], 4 * (VSM_MAX_PHYS_PAGES + 1), sizeof(UINT), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_PageLists[1]");

		VSM::CreateBuffer(device, context, m_RenderList, VSM_MAX_PHYS_PAGES, sizeof(RenderListEntry), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_RenderList");

		VSM::CreateBuffer(device, context, m_RenderCount, 1, sizeof(uint32), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_RenderCount");

		VSM::CreateBuffer(device, context, m_UncachedBounds, VSM_NUM_CLIPMAPS, sizeof(XMUINT4), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_UncachedBounds");

#if VSM_GENERATE_STATS
		VSM::CreateBuffer(device, context, m_StatsBuffer, VSM_STAT_NUM, sizeof(uint32), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_StatsBuffer");

		m_StatsReadbackBuffer = std::make_unique<ReadBackBufferD3D12>(device, STATS_RING * VSM_STAT_NUM * sizeof(uint32), QueueId::Direct);
#endif

		m_CbBuffers.ClipmapBuffer = m_ClipmapBuffer->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.PhysMeta = m_PhysMeta->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.PageRequest = m_PageRequest->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.PageTable = m_PageTable->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.RenderList = m_RenderList->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.RenderCount = m_RenderCount->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.UncachedBounds = m_UncachedBounds->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.PhysPool = m_PhysPool->GetUAVView()->GetGpuHeapIndex();
#if VSM_GENERATE_STATS
		m_CbBuffers.StatsBuffer = m_StatsBuffer->GetUAVView()->GetGpuHeapIndex();
#endif

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(device);
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(device);

		ShaderResourceDesc resDesc[]
		{
			ShaderResourceDesc("cbObject", SHADER_RESOURCE_TYPE_DYNAMIC),
			ShaderResourceDesc("cbPass", SHADER_RESOURCE_TYPE_DYNAMIC),
		};

		ShaderDesc shaderDesc = {};
		shaderDesc.DefaultType = SHADER_RESOURCE_TYPE_MUTABLE;
		shaderDesc.ResourceDesc = resDesc;
		shaderDesc.ResourceNum = _countof(resDesc);
		
		auto vsGeometry = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"VS_Geometry", L"vs_6_6", nullptr, shaderDesc);
		auto vsFSQuad = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_6", nullptr, shaderDesc);
		auto psGeometry = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"PS_Geometry", L"ps_6_6", nullptr, shaderDesc);
		auto psDrawShadow = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"PS_DrawShadow", L"ps_6_6", nullptr, shaderDesc);
		auto psDrawDebug = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"PS_DrawDebug", L"ps_6_6", nullptr, shaderDesc);

		UINT CS_BIND_PASS = 1 << 0;
		UINT CS_BIND_BUFFERS = 1 << 1;

		auto CreateCSPso = [&](CSPso& pso, const wchar_t* csEntry, const wchar_t* name, UINT bindRes = 0, const wchar_t* defines[] = nullptr)
			{
				auto cs = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", csEntry, L"cs_6_6", defines, shaderDesc);
				pso.PSO.SetShader(cs);
				pso.PSO.Build(device);
				pso.PSO.SetName(name);

				pso.Binder = pso.PSO.CreateShaderBinder();
				if (bindRes & CS_BIND_PASS)
					pso.Binder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
				if (bindRes & CS_BIND_BUFFERS)
					pso.Binder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbBuffers", m_ConstantsBuffer);
			};

		CreateCSPso(m_ClearPso, L"CS_Clear", L"m_ClearPso", CS_BIND_PASS | CS_BIND_BUFFERS);
		CreateCSPso(m_RemapPso, L"CS_RemapPhysicalPages", L"m_RemapPso", CS_BIND_PASS | CS_BIND_BUFFERS);
		CreateCSPso(m_MarkPso, L"CS_Mark", L"m_MarkPso", CS_BIND_PASS | CS_BIND_BUFFERS);
		CreateCSPso(m_UpdatePso, L"CS_UpdatePhysicalPages", L"m_UpdatePso", CS_BIND_PASS | CS_BIND_BUFFERS);
		CreateCSPso(m_PackPso, L"CS_PackAvailablePages", L"m_PackPso", CS_BIND_PASS | CS_BIND_BUFFERS);
		CreateCSPso(m_AllocatePso, L"CS_Allocate", L"m_AllocatePso", CS_BIND_PASS | CS_BIND_BUFFERS);
		CreateCSPso(m_FinalizePso, L"CS_FinalizeLists", L"m_FinalizePso", CS_BIND_PASS | CS_BIND_BUFFERS);
		CreateCSPso(m_DispatchArgsPso, L"CS_DispatchArgs", L"m_DispatchArgsPso", CS_BIND_BUFFERS);
		CreateCSPso(m_ClearSlotsPso, L"CS_ClearSlots", L"m_ClearSlotsPso", CS_BIND_BUFFERS);
#if VSM_GENERATE_STATS
		const wchar_t* defines[] = { L"VSM_GENERATE_STATS", L"1", NULL };
		CreateCSPso(m_ValidatePso, L"CS_ValidateLists", L"m_ValidatePso", CS_BIND_PASS | CS_BIND_BUFFERS, defines);
#endif

		D3D12_INPUT_ELEMENT_DESC inputLayout[] // TODO: take into account only POSITION
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = false;
		
		// Geometry PSO
		{
			m_GeometryPso.SetInputLayout({ inputLayout, _countof(inputLayout) });
			m_GeometryPso.SetDepthStencilState(dss);
			m_GeometryPso.SetShader(vsGeometry);
			m_GeometryPso.SetShader(psGeometry);
			m_GeometryPso.Build(device);
			m_GeometryPso.SetName(L"m_GeometryPso");

			m_GeometryBinder = m_GeometryPso.CreateShaderBinder();
			m_GeometryBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbObject", m_ObjBuffer);
			m_GeometryBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
			m_GeometryBinder->BindResource(EDU_SHADER_TYPE_VERTEX, "cbBuffers", m_ConstantsBuffer);
			m_GeometryBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbBuffers", m_ConstantsBuffer);
		}

		// Shadow Mask PSO
		{
			m_DrawShadowPso.SetRTVFormat(DXGI_FORMAT_R8_UNORM);
			m_DrawShadowPso.SetDepthStencilState(dss);
			m_DrawShadowPso.SetShader(vsFSQuad);
			m_DrawShadowPso.SetShader(psDrawShadow);
			m_DrawShadowPso.Build(device);
			m_DrawShadowPso.SetName(L"m_DrawShadowPso");

			m_DrawShadowBinder = m_DrawShadowPso.CreateShaderBinder();
			m_DrawShadowBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
			m_DrawShadowBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbBuffers", m_ConstantsBuffer);
		}

		// Debug Mask PSO
		{
			m_DrawDebugPso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
			m_DrawDebugPso.SetDepthStencilState(dss);
			m_DrawDebugPso.SetShader(vsFSQuad);
			m_DrawDebugPso.SetShader(psDrawDebug);
			m_DrawDebugPso.Build(device);
			m_DrawDebugPso.SetName(L"m_DrawDebugPso");

			m_DrawDebugBinder = m_DrawDebugPso.CreateShaderBinder();
			m_DrawDebugBinder->BindDynamicResource(EDU_SHADER_TYPE_PIXEL, "cbPass", m_PassBuffer);
			m_DrawDebugBinder->BindResource(EDU_SHADER_TYPE_PIXEL, "cbBuffers", m_ConstantsBuffer);
		}
	}

	void VirtualShadowMap::SetScreenSpaceInputs(RenderDeviceD3D12* device, DeviceContext* context, UINT ssDepthTex, UINT ssNormalTex, XMUINT2 screenSize)
	{
		m_ScreenSize = screenSize;

		m_CbBuffers.SSDepthTexture = ssDepthTex;
		m_CbBuffers.SSNormalTexture = ssNormalTex;
		m_ConstantsBuffer->LoadData(context, &m_CbBuffers);

		D3D12_RESOURCE_DESC buffDesc = {};
		buffDesc.Width = m_ScreenSize.x;
		buffDesc.Height = m_ScreenSize.y;
		buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		buffDesc.Alignment = 0;
		buffDesc.DepthOrArraySize = 1;
		buffDesc.MipLevels = 1;
		buffDesc.SampleDesc.Count = 1;
		buffDesc.SampleDesc.Quality = 0;
		buffDesc.Format = DXGI_FORMAT_R8_UNORM;
		buffDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		buffDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE optClear = {};
		optClear.Format = DXGI_FORMAT_R8_UNORM;
		optClear.Color[0] = 0.0f;
		optClear.Color[1] = 0.0f;
		optClear.Color[2] = 0.0f;
		optClear.Color[3] = 1.0f;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;

		m_ShadowMask = std::make_shared<TextureD3D12>(device, buffDesc, &optClear, QueueId::Direct);
		m_ShadowMask->CreateRTV(nullptr);
		m_ShadowMask->CreateSRV(&srvDesc, false);
		m_ShadowMask->SetName(L"m_ShadowMask");

		buffDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		optClear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

		m_DebugMask = std::make_shared<TextureD3D12>(device, buffDesc, &optClear, QueueId::Direct);
		m_DebugMask->CreateRTV(nullptr);
		m_DebugMask->CreateSRV(&srvDesc, false);
		m_DebugMask->SetName(L"m_DebugMask");

		context->GetCommandCtx()->TransitionResource(m_ShadowMask.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		context->GetCommandCtx()->TransitionResource(m_DebugMask.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void VirtualShadowMap::BuildCommandSignature(RenderDeviceD3D12* device, DeviceContext* context)
	{
		// ClearDispatchArgs
		{
			D3D12_DISPATCH_ARGUMENTS arg = {};
			arg.ThreadGroupCountX = 1;
			arg.ThreadGroupCountY = 1;
			arg.ThreadGroupCountZ = 1;

			auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_DISPATCH_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
			uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav.Buffer.NumElements = sizeof(D3D12_DISPATCH_ARGUMENTS) / sizeof(UINT);
			uav.Buffer.StructureByteStride = sizeof(UINT);

			m_ClearDispatchArgs = std::make_shared<BufferD3D12>(device, context, desc, &arg, QueueId::Direct);
			m_ClearDispatchArgs->CreateUAV(&uav, false);
			m_ClearDispatchArgs->SetName(L"m_ClearDispatchArgs");

			D3D12_INDIRECT_ARGUMENT_DESC dispatchArgsDesc = {};
			dispatchArgsDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

			m_ClearDispatchArgsSignature.SetByteStride(sizeof(D3D12_DISPATCH_ARGUMENTS));
			m_ClearDispatchArgsSignature.SetArguments(1, &dispatchArgsDesc);
			m_ClearDispatchArgsSignature.Build(device);
			m_ClearDispatchArgsSignature.SetName(L"m_ClearDispatchArgsSignature");
		}

		m_CbBuffers.ClearDispatchArgs = m_ClearDispatchArgs->GetUAVView()->GetGpuHeapIndex();
		m_ConstantsBuffer->LoadData(context, &m_CbBuffers);
	}

	void VirtualShadowMap::Update(DeviceContext* context, Camera* camera, Light* light)
	{
		struct PassData
		{
			XMFLOAT4X4 InvView;
			XMFLOAT4X4 InvProj;
			XMFLOAT3 CameraWorldPos;
			float ResolutionLodBias;
			XMUINT2 ScreenSize;
			UINT Frame;
			UINT InvalidateAll;
			UINT CurList;
			UINT PrevList;
			UINT HasHistory;
			UINT DebugMode;
		} passData = {};

		XMMATRIX ligthView;
		XMMATRIX ligthProj;

		XMStoreFloat4x4(&passData.InvView, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetViewMatrix()))));
		XMStoreFloat4x4(&passData.InvProj, XMMatrixTranspose(XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetProjectionMatrix()))));

		// View
		{
			XMVECTOR lightPos = XMVectorZero();
			XMVECTOR lightDir = XMLoadFloat3(&light->Direction);
			XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

			if (XMVector3NearEqual(XMLoadFloat3(&light->Direction), upVector, g_XMEpsilon) ||
				XMVector3NearEqual(XMLoadFloat3(&light->Direction), XMVectorNegate(upVector), g_XMEpsilon))
			{
				upVector = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			}

			ligthView = XMMatrixLookAtLH(lightPos, XMVectorAdd(lightPos, lightDir), upVector);
		}

        std::array<ClipmapLevelGPU, VSM_NUM_CLIPMAPS> clipmapsGpu = {};
        XMFLOAT3 cameraLS;
        XMStoreFloat3(&cameraLS, XMVector3TransformCoord(XMLoadFloat3(&camera->GetPosition()), ligthView));
        
		const XMMATRIX uvTransform(
            0.5f, 0.0f,	0.0f, 0.0f,
			0.0f,-0.5f,	0.0f, 0.0f,
			0.0f, 0.0f,	1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);
        
		for (uint32 i = 0; i < VSM_NUM_CLIPMAPS; ++i)
        {
            const float radius = std::exp2(float(VSM_FIRST_CLIPMAP_LEVEL + i + 1));
            const float halfSize = 2.0f * radius;
            const XMINT2 snap(int(std::round(cameraLS.x / radius)), int(std::round(cameraLS.y / radius)));
            const float centerX = snap.x * radius;
            const float centerY = snap.y * radius;

            ligthProj = XMMatrixOrthographicOffCenterLH(
				centerX - halfSize, // l
				centerX + halfSize, // r
                centerY - halfSize, // b
				centerY + halfSize, // t
				500.0f,				// f
				-500.0f);			// n

            auto& level = clipmapsGpu[i];
            level.AbsoluteLevel = VSM_FIRST_CLIPMAP_LEVEL + i;
            level.WorldTexelSize = 2.0f * halfSize / VSM_VIRTUAL_SIZE;
            level.WindowOriginPage = XMINT2(snap.x * 32, -snap.y * 32);
            level.PageOffset = m_Frame == 0 ? 
				XMINT2(0, 0) :
                XMINT2(-32 * (snap.x - m_PrevSnap[i].x), 32 * (snap.y - m_PrevSnap[i].y));

            XMStoreFloat4x4(&level.WorldToShadowUV, XMMatrixTranspose(ligthView * ligthProj * uvTransform));
            XMStoreFloat4x4(&level.WorldToShadowClip, XMMatrixTranspose(ligthView * ligthProj));
            m_PrevSnap[i] = snap;
        }
		
        UINT clipmapBytes = sizeof(clipmapsGpu);
        m_ClipmapBuffer->LoadData(context, clipmapsGpu.data(), &clipmapBytes);
        passData.CameraWorldPos = camera->GetPosition();
        const float projectionScaleX = camera->GetProjectionMatrix()._11;
        passData.ResolutionLodBias = std::max(0.0f, std::log2(0.5f / projectionScaleX * float(VSM_VIRTUAL_SIZE) / float(m_ScreenSize.x)));

		if (m_Frame == 0)
			m_PrevLightDir = light->Direction;

		bool lightRotChanged = memcmp(&m_PrevLightDir, &(light->Direction), sizeof(XMFLOAT3));

		m_CurListIdx = m_Frame & 1;
		m_PrevListIdx = m_CurListIdx ^ 1;

		passData.ScreenSize = m_ScreenSize;
		passData.Frame = m_Frame;
		passData.InvalidateAll = (m_Frame == 0) || lightRotChanged;
		passData.CurList = m_PageLists[m_CurListIdx]->GetUAVView()->GetGpuHeapIndex();
		passData.PrevList = m_PageLists[m_PrevListIdx]->GetUAVView()->GetGpuHeapIndex();
		passData.HasHistory = m_Frame > 0;
		passData.DebugMode = (UINT)m_DebugMode;

		m_PassBuffer->LoadData(context, passData);

		m_PrevLightDir = light->Direction;
		m_Frame++;
	}

	void VirtualShadowMap::Compute(DeviceContext* context, RenderObject* objects, uint32 objectsNum)
	{
		VERIFY_EXPR(m_ShadowMask != nullptr, "ShadowMask texture not created");
		VERIFY_EXPR(m_ClearDispatchArgsSignature.GetD3D12Signature() != nullptr, "ClearDispatchArgsSignature not created");

		auto* ctx = context->GetCommandCtx();
		auto* cmd = ctx->GetCmdList();

		ctx->TransitionResource(m_ClipmapBuffer.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_PhysPool.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_PhysMeta.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_PageRequest.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_PageTable.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_PageLists[0].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_PageLists[1].get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_RenderList.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_RenderCount.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_UncachedBounds.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_ClearDispatchArgs.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
#if VSM_GENERATE_STATS
		ctx->TransitionResource(m_StatsBuffer.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
#endif
		ctx->FlushResourceBarriers();

		// CS_Clear
		{
			UINT groupCount = static_cast<UINT>(ceil(VSM_PAGES_XY / 8.0f));
			m_ClearPso.PSO.BeginPSOAndCommitResources(context, m_ClearPso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, groupCount * VSM_NUM_CLIPMAPS, 1);
		}

		// CS_RemapPhysicalPages
		{
			UINT groupCount = static_cast<UINT>(ceil(VSM_MAX_PHYS_PAGES / 32.0f));
			m_RemapPso.PSO.BeginPSOAndCommitResources(context, m_RemapPso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, 1, 1);
		}

#if VSM_GENERATE_STATS
		ctx->InsertUAVBarrier(m_StatsBuffer.get());
#endif
		ctx->InsertUAVBarrier(m_PageRequest.get(), true);

		// CS_Mark
		{
			UINT groupCountX = static_cast<UINT>(ceil(m_ScreenSize.x / 8.0f));
			UINT groupCountY = static_cast<UINT>(ceil(m_ScreenSize.y / 8.0f));
			m_MarkPso.PSO.BeginPSOAndCommitResources(context, m_MarkPso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCountX, groupCountY, 1);
		}

		ctx->InsertUAVBarrier(m_PageLists[m_CurListIdx].get());
		ctx->InsertUAVBarrier(m_PhysMeta.get());
		ctx->InsertUAVBarrier(m_PageRequest.get());
		ctx->InsertUAVBarrier(m_PageTable.get(), true);

		// CS_UpdatePhysicalPages
		{
			UINT groupCount = static_cast<UINT>(ceil(VSM_MAX_PHYS_PAGES / 32.0f));
			m_UpdatePso.PSO.BeginPSOAndCommitResources(context, m_UpdatePso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, 1, 1);
		}

		ctx->InsertUAVBarrier(m_PageLists[m_CurListIdx].get(), true);

		// CS_PackAvailablePages
		{
			m_PackPso.PSO.BeginPSOAndCommitResources(context, m_PackPso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(1, 1, 1);
		}

		ctx->InsertUAVBarrier(m_PageLists[m_CurListIdx].get());
		ctx->InsertUAVBarrier(m_RenderCount.get());
		ctx->InsertUAVBarrier(m_PhysMeta.get());
		ctx->InsertUAVBarrier(m_UncachedBounds.get());
		ctx->InsertUAVBarrier(m_PageTable.get(), true);

		// CS_Allocate
		{
			UINT groupCount = static_cast<UINT>(ceil(VSM_PAGES_XY / 8.0f));
			m_AllocatePso.PSO.BeginPSOAndCommitResources(context, m_AllocatePso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, groupCount * VSM_NUM_CLIPMAPS, 1);
		}

		ctx->InsertUAVBarrier(m_PageLists[m_CurListIdx].get(), true);

		// CS_FinalizeLists
		{
			m_FinalizePso.PSO.BeginPSOAndCommitResources(context, m_FinalizePso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(1, 1, 1);
		}

#if VSM_GENERATE_STATS
		ctx->InsertUAVBarrier(m_PageLists[m_CurListIdx].get(), true);

		// CS_ValidateLists
		{
			m_ValidatePso.PSO.BeginPSOAndCommitResources(context, m_ValidatePso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(1, 1, 1);
		}
#endif

		ctx->InsertUAVBarrier(m_RenderList.get());
		ctx->InsertUAVBarrier(m_RenderCount.get(), true);

		// DispatchArgs
		{
			m_DispatchArgsPso.PSO.BeginPSOAndCommitResources(context, m_DispatchArgsPso.Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(1, 1, 1);
		}
		
		ctx->TransitionResource(m_ClearDispatchArgs.get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		ctx->FlushResourceBarriers();

		// Clear Slots
		{
			m_ClearSlotsPso.PSO.BeginPSOAndCommitResources(context, m_ClearSlotsPso.Binder.get());

			cmd->ExecuteIndirect(m_ClearDispatchArgsSignature.GetD3D12Signature(),
				1,
				m_ClearDispatchArgs->GetD3D12Resource(),
				0,
				nullptr, 0);
		}

		ctx->InsertUAVBarrier(m_PageTable.get());
		ctx->InsertUAVBarrier(m_UncachedBounds.get());
		ctx->InsertUAVBarrier(m_PhysPool.get(), true);

		// Draw Depth
		{
			D3D12_VIEWPORT vp = { 0, 0, VSM_VIRTUAL_SIZE, VSM_VIRTUAL_SIZE, 0, 1 };
			D3D12_RECT sc = { 0, 0, VSM_VIRTUAL_SIZE, VSM_VIRTUAL_SIZE };
			cmd->RSSetViewports(1, &vp);
			cmd->RSSetScissorRects(1, &sc);

			ctx->SetRenderTargets(0, nullptr, false, nullptr);

			m_GeometryPso.BeginPSO(context);

			for (uint32 obj = 0; obj < objectsNum; obj++)
			{
				struct ObjData
				{
					XMFLOAT4X4 World;
				} objData;

				XMStoreFloat4x4(&objData.World, XMMatrixTranspose(objects[obj].World));
				m_ObjBuffer->LoadData(context, objData);

				for (uint32 mIdx = 0; mIdx < objects[obj].Mesh->GetMeshCount(); mIdx++)
				{
					m_GeometryPso.CommitResources(context, m_GeometryBinder.get());
					cmd->IASetIndexBuffer(&objects[obj].Mesh->GetIndexBuffer(mIdx)->GetView());
					cmd->IASetVertexBuffers(0, 1, &objects[obj].Mesh->GetVertexBuffer(mIdx)->GetView());

					cmd->DrawIndexedInstanced(objects[obj].Mesh->GetIndexCount(mIdx), VSM_NUM_CLIPMAPS, 0, 0, 0);
				}
			}
		}

		ctx->InsertUAVBarrier(m_PhysPool.get());
		ctx->TransitionResource(m_ShadowMask.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		// Draw ShadowMask
		{
			D3D12_VIEWPORT vp = { 0, 0, m_ScreenSize.x, m_ScreenSize.y, 0, 1 };
			D3D12_RECT sc = { 0, 0, m_ScreenSize.x, m_ScreenSize.y };
			cmd->RSSetViewports(1, &vp);
			cmd->RSSetScissorRects(1, &sc);
			ctx->SetRenderTargets(1, &m_ShadowMask->GetRTVView()->GetCpuHandle(), false, nullptr);

			m_DrawShadowPso.BeginPSOAndCommitResources(context, m_DrawShadowBinder.get());
			context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
		}

		// Draw DebugMask
		if (m_DebugMode != VSMDebugMode::None)
		{
			ctx->TransitionResource(m_DebugMask.get(), D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			ctx->SetRenderTargets(1, &m_DebugMask->GetRTVView()->GetCpuHandle(), false, nullptr);

			m_DrawDebugPso.BeginPSOAndCommitResources(context, m_DrawDebugBinder.get());
			context->GetCommandCtx()->GetCmdList()->DrawInstanced(3, 1, 0, 0);
			
			ctx->TransitionResource(m_DebugMask.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		ctx->TransitionResource(m_ShadowMask.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

#if VSM_GENERATE_STATS
		//
		// Read Stats
		//
		const uint32 size = VSM_STAT_NUM * sizeof(uint32);
		const uint32 slot = m_Frame % STATS_RING;
		m_StatsReadbackBuffer->CopyBuffer(context, m_StatsBuffer.get(), 0, slot * size, size);

		if (m_Frame >= STATS_RING)
		{
			const uint32 readSlot = (m_Frame + 1) % STATS_RING;
			m_StatsReadbackBuffer->ReadData(m_StatsData.data(), size, readSlot * size);
		}
#endif
	}
}
