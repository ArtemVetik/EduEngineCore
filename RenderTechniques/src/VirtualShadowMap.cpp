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
		m_ClearPso(QueueId::Direct),
		m_UpdatePso(QueueId::Direct),
		m_MarkPso(QueueId::Direct),
		m_AllocatePso(QueueId::Direct),
		m_DrawArgsPso(QueueId::Direct),
		m_Frame(0)
	{
		VSM::CreateBuffer(device, context, m_ConstantsBuffer, 1, sizeof(CbBuffers), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE,
			{ false, DXGI_FORMAT_UNKNOWN }, L"m_ConstantsBuffer");

		struct PhysMetaEntry
		{
			UINT Flags;
			UINT LastUsedFrame;
			UINT AllocFrame;
			XMUINT2 PageAddr;
		};

		struct RenderListEntry
		{
			UINT Slot;
			XMUINT2 PageAddr;
		};

		VSM::CreateTexture(device, m_PhysPool, POOL_SIZE * PAGE_SIZE, POOL_SIZE * PAGE_SIZE, DXGI_FORMAT_R24G8_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
			{ true, DXGI_FORMAT_D24_UNORM_S8_UINT },	 // clear
			{ true, DXGI_FORMAT_R24_UNORM_X8_TYPELESS }, // srv
			{ true, DXGI_FORMAT_D24_UNORM_S8_UINT },	 // dsv
			{ false, DXGI_FORMAT_UNKNOWN },				 // uav
			L"m_PhysPool");

		VSM::CreateBuffer(device, context, m_PhysMeta, POOL_SIZE * POOL_SIZE, sizeof(PhysMetaEntry), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_PhysMeta");

		VSM::CreateTexture(device, m_PageTable, PAGES_XY, PAGES_XY, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ false, DXGI_FORMAT_UNKNOWN },	// clear
			{ false, DXGI_FORMAT_UNKNOWN }, // srv
			{ false, DXGI_FORMAT_UNKNOWN },	// dsv
			{ true, DXGI_FORMAT_R32_UINT },	// uav
			L"m_PageTable");

		VSM::CreateBuffer(device, context, m_FreeList, POOL_SIZE * POOL_SIZE + 1, sizeof(uint32), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_FreeList");

		VSM::CreateBuffer(device, context, m_RenderList, POOL_SIZE * POOL_SIZE, sizeof(RenderListEntry), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_RenderList");

		VSM::CreateBuffer(device, context, m_RenderCount, 1, sizeof(uint32), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_RenderCount");

		VSM::CreateBuffer(device, context, m_AllocCount, 1, sizeof(uint32), DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
			{ true, DXGI_FORMAT_UNKNOWN }, L"m_AllocCount");

		context->GetCommandCtx()->TransitionResource(m_PhysPool.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

		m_CbBuffers.PhysPool = m_PhysPool->GetSRVView()->GetGpuHeapIndex();
		m_CbBuffers.PhysMeta = m_PhysMeta->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.PageTable = m_PageTable->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.FreeList = m_FreeList->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.RenderList = m_RenderList->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.RenderCount = m_RenderCount->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.AllocCount = m_AllocCount->GetUAVView()->GetGpuHeapIndex();

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

		auto csClear = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"CS_Clear", L"cs_6_6", nullptr, shaderDesc);
		auto csUpdatePhysicalPages = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"CS_UpdatePhysicalPages", L"cs_6_6", nullptr, shaderDesc);
		auto csMark = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"CS_Mark", L"cs_6_6", nullptr, shaderDesc);
		auto csAllocate = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"CS_Allocate", L"cs_6_6", nullptr, shaderDesc);
		auto csDrawArgs = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"CS_DrawArgs", L"cs_6_6", nullptr, shaderDesc);
		
		auto vsClearSlots = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"VS_ClearSlots", L"vs_6_6", nullptr, shaderDesc);
		auto vsGeometry = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"VS_Geometry", L"vs_6_6", nullptr, shaderDesc);
		auto vsFSQuad = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\FSQuadVS.hlsl", L"VS", L"vs_6_6", nullptr, shaderDesc);
		
		auto psDrawShadow = ShaderCache::Get().GetOrCreate(L"assets\\Shaders\\VirtualShadowMap.hlsl", L"PS_DrawShadow", L"ps_6_6", nullptr, shaderDesc);

		m_ClearPso.SetShader(csClear);
		m_ClearPso.Build(device);
		m_ClearPso.SetName(L"m_ClearPso");

		m_ClearBinder = m_ClearPso.CreateShaderBinder();
		m_ClearBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbBuffers", m_ConstantsBuffer);

		m_UpdatePso.SetShader(csUpdatePhysicalPages);
		m_UpdatePso.Build(device);
		m_UpdatePso.SetName(L"m_UpdatePso");

		m_UpdateBinder = m_UpdatePso.CreateShaderBinder();
		m_UpdateBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
		m_UpdateBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbBuffers", m_ConstantsBuffer);

		m_MarkPso.SetShader(csMark);
		m_MarkPso.Build(device);
		m_MarkPso.SetName(L"m_MarkPso");

		m_MarkBinder = m_MarkPso.CreateShaderBinder();
		m_MarkBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
		m_MarkBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbBuffers", m_ConstantsBuffer);

		m_AllocatePso.SetShader(csAllocate);
		m_AllocatePso.Build(device);
		m_AllocatePso.SetName(L"m_AllocatePso");

		m_AllocateBinder = m_AllocatePso.CreateShaderBinder();
		m_AllocateBinder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
		m_AllocateBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbBuffers", m_ConstantsBuffer);

		m_DrawArgsPso.SetShader(csDrawArgs);
		m_DrawArgsPso.Build(device);
		m_DrawArgsPso.SetName(L"m_DrawArgsPso");

		m_DrawArgsBinder = m_DrawArgsPso.CreateShaderBinder();
		m_DrawArgsBinder->BindResource(EDU_SHADER_TYPE_COMPUTE, "cbBuffers", m_ConstantsBuffer);

		D3D12_INPUT_ELEMENT_DESC inputLayout[] // TODO: take into account only POSITION
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		// Clear Slots PSO
		{
			D3D12_RASTERIZER_DESC rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			rast.CullMode = D3D12_CULL_MODE_NONE;

			m_ClearSlotsPso.SetShader(vsClearSlots);
			m_ClearSlotsPso.SetDepthStencilState(dss);
			m_ClearSlotsPso.SetRasterizerState(rast);
			m_ClearSlotsPso.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, true);
			m_ClearSlotsPso.Build(device);
			m_ClearSlotsPso.SetName(L"m_ClearSlotsPso");

			m_ClearSlotsBinder = m_ClearSlotsPso.CreateShaderBinder();
			m_ClearSlotsBinder->BindResource(EDU_SHADER_TYPE_VERTEX, "cbBuffers", m_ConstantsBuffer);
		}

		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		
		// Geometry PSO
		{
			D3D12_RASTERIZER_DESC rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			rast.DepthBias = -1;
			rast.DepthBiasClamp = 0.0f;
			rast.SlopeScaledDepthBias = -2.5f;

			m_GeometryPso.SetInputLayout({ inputLayout, _countof(inputLayout) });
			m_GeometryPso.SetDepthStencilState(dss);
			m_GeometryPso.SetRasterizerState(rast); // TODO: verify rast state
			m_GeometryPso.SetShader(vsGeometry);
			m_GeometryPso.Build(device);
			m_GeometryPso.SetName(L"m_GeometryPso");

			m_GeometryBinder = m_GeometryPso.CreateShaderBinder();
			m_GeometryBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbObject", m_ObjBuffer);
			m_GeometryBinder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
			m_GeometryBinder->BindResource(EDU_SHADER_TYPE_VERTEX, "cbBuffers", m_ConstantsBuffer);
		}

		dss.DepthEnable = false;

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
	}

	void VirtualShadowMap::SetSSDepth(RenderDeviceD3D12* device, DeviceContext* context, UINT ssDepthTex, XMUINT2 screenSize)
	{
		m_ScreenSize = screenSize;

		m_CbBuffers.SSDepthTexture = ssDepthTex;
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

		context->GetCommandCtx()->TransitionResource(m_ShadowMask.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	void VirtualShadowMap::BuildCommandSignature(RenderDeviceD3D12* device, DeviceContext* context, RenderObject* objects, uint32 objectsNum)
	{
		std::vector<D3D12_DRAW_INDEXED_ARGUMENTS> drawArgsData;

		// DrawArgs
		{
			for (uint32 obj = 0; obj < objectsNum; obj++)
			{
				for (uint32 mIdx = 0; mIdx < objects[obj].Mesh->GetMeshCount(); mIdx++)
				{
					D3D12_DRAW_INDEXED_ARGUMENTS arg = {};
					arg.BaseVertexLocation = 0;
					arg.StartIndexLocation = 0;
					arg.StartInstanceLocation = 0;
					arg.InstanceCount = 0;
					arg.IndexCountPerInstance = objects[obj].Mesh->GetIndexCount(mIdx);

					drawArgsData.push_back(arg);
				}
			}

			auto desc = CD3DX12_RESOURCE_DESC::Buffer(drawArgsData.size() * sizeof(drawArgsData[0]), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
			uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav.Buffer.NumElements = drawArgsData.size() * sizeof(drawArgsData[0]) / sizeof(UINT);
			uav.Buffer.StructureByteStride = sizeof(UINT);

			m_DrawArgs = std::make_shared<BufferD3D12>(device, context, desc, drawArgsData.data(), QueueId::Direct);
			m_DrawArgs->CreateUAV(&uav, false);
			m_DrawArgs->SetName(L"m_DrawArgs");

			D3D12_INDIRECT_ARGUMENT_DESC drawArgsDesc = {};
			drawArgsDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

			m_DrawArgsSignature.SetByteStride(sizeof(D3D12_DRAW_INDEXED_ARGUMENTS));
			m_DrawArgsSignature.SetArguments(1, &drawArgsDesc);
			m_DrawArgsSignature.Build(device);
			m_DrawArgsSignature.SetName(L"m_DrawArgsSignature");
		}

		// QuadDrawArgs
		{
			D3D12_DRAW_ARGUMENTS arg = {};
			arg.VertexCountPerInstance = 4;
			arg.InstanceCount = 0;
			arg.StartVertexLocation = 0;
			arg.StartInstanceLocation = 0;

			auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_DRAW_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

			D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
			uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uav.Buffer.NumElements = sizeof(D3D12_DRAW_ARGUMENTS) / sizeof(UINT);
			uav.Buffer.StructureByteStride = sizeof(UINT);

			m_QuadDrawArgs = std::make_shared<BufferD3D12>(device, context, desc, &arg, QueueId::Direct);
			m_QuadDrawArgs->CreateUAV(&uav, false);
			m_QuadDrawArgs->SetName(L"m_QuadDrawArgs");

			D3D12_INDIRECT_ARGUMENT_DESC drawArgsDesc = {};
			drawArgsDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

			m_QuadDrawArgsSignature.SetByteStride(sizeof(D3D12_DRAW_ARGUMENTS));
			m_QuadDrawArgsSignature.SetArguments(1, &drawArgsDesc);
			m_QuadDrawArgsSignature.Build(device);
			m_QuadDrawArgsSignature.SetName(L"m_QuadDrawArgsSignature");
		}

		m_CbBuffers.QuadDrawArgs = m_QuadDrawArgs->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.DrawArgs = m_DrawArgs->GetUAVView()->GetGpuHeapIndex();
		m_CbBuffers.DrawCount = drawArgsData.size();
		m_ConstantsBuffer->LoadData(context, &m_CbBuffers);
	}

	void VirtualShadowMap::Update(DeviceContext* context, Camera* camera, Light* light)
	{
		struct PassData
		{
			XMFLOAT4X4 InvView;
			XMFLOAT4X4 InvProj;
			XMFLOAT4X4 LightViewProj;
			XMFLOAT4X4 LightViewProjNDC;
			XMUINT2 ScreenSize;
			XMINT2 PageOffset;
			UINT Frame;
			UINT InvalidateAll;
			XMINT2 WindowOriginPage;
		} passData;

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

		// Projection
		{
			const float halfSize = 1024;
			const float pageWorld = 2 * halfSize / PAGES_XY;

			XMFLOAT3 camLS;
			XMStoreFloat3(&camLS, XMVector3TransformCoord(XMLoadFloat3(&camera->GetPosition()), ligthView));

			XMINT2 camPage = { (int)floor(camLS.x / pageWorld), (int)floor(camLS.y / pageWorld) };

			if (m_Frame == 0)
				m_PrevPage = camPage;

			float l = -halfSize + camPage.x * pageWorld;
			float r = halfSize + camPage.x * pageWorld;
			float b = -halfSize + camPage.y * pageWorld;
			float t = halfSize + camPage.y * pageWorld;
			float n = -500;
			float f = 500;

			ligthProj = DirectX::XMMatrixOrthographicOffCenterLH(l, r, b, t, f, n);

			XMINT2 d = { camPage.x - m_PrevPage.x, camPage.y - m_PrevPage.y };
			passData.PageOffset = XMINT2(-d.x, +d.y);
			passData.WindowOriginPage = XMINT2(camPage.x, -camPage.y);

			m_PrevPage = camPage;
		}

		DirectX::XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

		XMStoreFloat4x4(&passData.LightViewProj, XMMatrixTranspose(ligthView* ligthProj* T));
		XMStoreFloat4x4(&passData.LightViewProjNDC, XMMatrixTranspose(ligthView* ligthProj));

		if (m_Frame == 0)
			m_PrevLightDir = light->Direction;

		bool lightRotChanged = memcmp(&m_PrevLightDir, &(light->Direction), sizeof(XMFLOAT3));

		passData.ScreenSize = m_ScreenSize;
		passData.Frame = m_Frame;
		passData.InvalidateAll = (m_Frame == 0) || lightRotChanged;

		m_PassBuffer->LoadData(context, passData);

		m_PrevLightDir = light->Direction;
		m_Frame++;
	}

	void VirtualShadowMap::Compute(DeviceContext* context, RenderObject* objects, uint32 objectsNum)
	{
		VERIFY_EXPR(m_ShadowMask != nullptr, "ShadowMask texture not created");
		VERIFY_EXPR(m_DrawArgsSignature.GetD3D12Signature() != nullptr, "DrawArgsSignature not created");
		VERIFY_EXPR(m_QuadDrawArgsSignature.GetD3D12Signature() != nullptr, "QuadDrawArgsSignature not created");

		auto* ctx = context->GetCommandCtx();
		auto* cmd = ctx->GetCmdList();

		ctx->TransitionResource(m_PhysMeta.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_PageTable.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_FreeList.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_RenderList.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_RenderCount.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_AllocCount.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_DrawArgs.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->TransitionResource(m_QuadDrawArgs.get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		ctx->FlushResourceBarriers();

		// Clear
		{
			UINT groupCount = static_cast<UINT>(ceil(PAGES_XY / 8.0f));
			m_ClearPso.BeginPSOAndCommitResources(context, m_ClearBinder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, groupCount, 1);
		}

		ctx->InsertUAVBarrier(m_PageTable.get());
		ctx->InsertUAVBarrier(m_FreeList.get(), true);

		// UpdateMeta
		{
			UINT groupCount = static_cast<UINT>(ceil(POOL_SIZE * POOL_SIZE / 32.0f));
			m_UpdatePso.BeginPSOAndCommitResources(context, m_UpdateBinder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, 1, 1);
		}

		ctx->InsertUAVBarrier(m_PageTable.get(), true);

		// Mark
		{
			UINT groupCountX = static_cast<UINT>(ceil(m_ScreenSize.x / 8.0f));
			UINT groupCountY = static_cast<UINT>(ceil(m_ScreenSize.y / 8.0f));
			m_MarkPso.BeginPSOAndCommitResources(context, m_MarkBinder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCountX, groupCountY, 1);
		}

		ctx->InsertUAVBarrier(m_AllocCount.get());
		ctx->InsertUAVBarrier(m_FreeList.get());
		ctx->InsertUAVBarrier(m_PhysMeta.get());
		ctx->InsertUAVBarrier(m_RenderCount.get());
		ctx->InsertUAVBarrier(m_PageTable.get(), true);

		// Allocate
		{
			UINT groupCount = static_cast<UINT>(ceil(PAGES_XY / 8.0f));
			m_AllocatePso.BeginPSOAndCommitResources(context, m_AllocateBinder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, groupCount, 1);
		}

		ctx->InsertUAVBarrier(m_RenderList.get());
		ctx->InsertUAVBarrier(m_RenderCount.get(), true);

		// DrawArgs
		{
			UINT groupCount = static_cast<UINT>(ceil(m_CbBuffers.DrawCount / 32.0f));
			m_DrawArgsPso.BeginPSOAndCommitResources(context, m_DrawArgsBinder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch(groupCount, 1, 1);
		}
		
		ctx->TransitionResource(m_PhysPool.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		ctx->TransitionResource(m_QuadDrawArgs.get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		ctx->TransitionResource(m_DrawArgs.get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
		ctx->FlushResourceBarriers();

		D3D12_VIEWPORT vp = { 0, 0, POOL_SIZE * PAGE_SIZE, POOL_SIZE * PAGE_SIZE, 0, 1 };
		D3D12_RECT sc = { 0, 0, vp.Width, vp.Height };
		cmd->RSSetViewports(1, &vp);
		cmd->RSSetScissorRects(1, &sc);
		ctx->SetRenderTargets(0, nullptr, false, &m_PhysPool->GetDSVView()->GetCpuHandle());

		// Clear Slots
		{
			m_ClearSlotsPso.BeginPSOAndCommitResources(context, m_ClearSlotsBinder.get());

			cmd->ExecuteIndirect(m_QuadDrawArgsSignature.GetD3D12Signature(),
				1,
				m_QuadDrawArgs->GetD3D12Resource(),
				0,
				nullptr, 0);
		}

		// Draw Depth
		{
			m_GeometryPso.BeginPSO(context);
			uint32 drawIdx = 0;

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

					cmd->ExecuteIndirect(m_DrawArgsSignature.GetD3D12Signature(),
						1,
						m_DrawArgs->GetD3D12Resource(),
						drawIdx * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS),
						nullptr, 0);
					++drawIdx;
				}
			}
		}

		ctx->TransitionResource(m_ShadowMask.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		ctx->TransitionResource(m_PhysPool.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

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

		ctx->TransitionResource(m_ShadowMask.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}