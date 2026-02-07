#include "ReflectionProbe.h"

namespace EduEngine
{
	ReflectionProbe::ReflectionProbe(RenderDeviceD3D12* device, DeviceContext* context)
	{
		m_Center = { 0, 10, 0 };
		m_BoxSize = { 100, 100 };

		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Width = 1024;
		texDesc.Height = 1024;
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
		m_ReflectionCube->CreateSRV(&srvDesc);
		m_ReflectionCube->SetName(L"m_ReflectionCube");

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2D.MipSlice = 0;

		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		m_DepthBuffer = std::make_shared<TextureD3D12>(device, texDesc, &optClear, QueueId::Direct);
		m_DepthBuffer->CreateDSV(&dsvDesc);

		context->GetCommandCtx()->TransitionResource(m_ReflectionCube.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		context->GetCommandCtx()->TransitionResource(m_DepthBuffer.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

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
		m_PSO.SetInputLayout({ inputLayout, _countof(inputLayout)});
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
	}

	void ReflectionProbe::Render(DeviceContext* context, RenderObject* renderObjects, uint32 objectsNum)
	{
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1, 100.0f, 0.1f);
		
		XMVECTOR center = XMLoadFloat3(&m_Center);

		XMMATRIX view[6]
		{
			XMMatrixLookToLH(center, {1,0,0}, {0,1,0}),   // +X
			XMMatrixLookToLH(center, {-1,0,0}, {0,1,0}),  // -X
			XMMatrixLookToLH(center, {0,1,0}, {0,0,-1}),  // +Y
			XMMatrixLookToLH(center, {0,-1,0}, {0,0,1}),  // -Y
			XMMatrixLookToLH(center, {0,0,1}, {0,1,0}),   // +Z
			XMMatrixLookToLH(center, {0,0,-1}, {0,1,0}),  // -Z
		};

		CommandContext* commandContext = context->GetCommandCtx();

		for (size_t i = 0; i < 6; i++)
		{
			D3D12_VIEWPORT viewport = {};
			viewport.Width = 1024;
			viewport.Height = 1024;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.MinDepth = 0;
			viewport.MaxDepth = 1;

			D3D12_RECT scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

			commandContext->SetViewports(&viewport, 1);
			commandContext->SetScissorRects(&scissorRect, 1);

			commandContext->SetRenderTargets(1, &m_ReflectionCube->GetRTVView()->GetCpuHandle(i), false, &m_DepthBuffer->GetDSVView()->GetCpuHandle());

			commandContext->GetCmdList()->ClearDepthStencilView(m_DepthBuffer->GetDSVView()->GetCpuHandle(),
				D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

			struct PassData
			{
				XMFLOAT4X4 ViewProj;
			} passData;

			XMStoreFloat4x4(&passData.ViewProj, XMMatrixTranspose(view[i] * proj));

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
		}
	}
}