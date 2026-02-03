#include "CSMRendering.h"

#include <DynamicUploadBuffer.h>
#include <Asserts.h>

namespace EduEngine
{
	CSMRendering::CSMRendering(RenderDeviceD3D12* device, DeviceContext* context) :
		m_Device(device),
		m_CascadeCount(_countof(CSMSizes))
	{
		for (int i = 0; i < m_CascadeCount; i++)
			m_CascadeSplits[i] = CSMSplits[i];

		D3D12_RESOURCE_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.DepthOrArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear = {};
		optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;

		for (size_t i = 0; i < m_CascadeCount; i++)
		{
			texDesc.Width = CSMSizes[i].x;
			texDesc.Height = CSMSizes[i].y;

			m_ShadowMaps[i] = std::make_unique<TextureD3D12>(m_Device, texDesc, &optClear, QueueId::Direct);
			m_ShadowMaps[i]->CreateDSV(&dsvDesc);

			context->GetCommandCtx()->TransitionResource(m_ShadowMaps[i].get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		for (size_t i = 0; i < m_CascadeCount; i++)
			m_ShadowMaps[i]->CreateSRV(&srvDesc);

		//
		// Build PSO
		//

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;
		sDesc.ResourceNum = 0;

		std::shared_ptr<ShaderD3D12> VS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\Shadows.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);

		D3D12_INPUT_ELEMENT_DESC inputLayout[] // TODO: take into account only POSITION
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_RASTERIZER_DESC rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rast.DepthBias = 30000;
		rast.DepthBiasClamp = 0.0f;
		rast.SlopeScaledDepthBias = 1.0f;

		m_Pso.SetInputLayout({ inputLayout, _countof(inputLayout) });
		m_Pso.SetRasterizerState(rast);
		m_Pso.SetShader(VS);
		m_Pso.Build(device);
		m_Pso.SetName(L"ShadowMapPSO");

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		m_Binder = m_Pso.CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
	}

	void CSMRendering::Render(DeviceContext* context, Camera* camera, Light* light, const Mesh* mesh)
	{
		XMMATRIX lightView = CalculateLightView(light);

		float cascadeNear = 0.0f;

		auto* commandContext = context->GetCommandCtx();

		for (int i = 0; i < m_CascadeCount; i++)
		{
			float cascadeFar = m_CascadeSplits[i] * ShadowDistance;

			XMMATRIX lightProj = CalculateCascadeProjection(cascadeNear, cascadeFar, camera, lightView);

			cascadeNear = cascadeFar;

			// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
			DirectX::XMMATRIX T(
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, -0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.5f, 0.5f, 0.0f, 1.0f);

			m_CascadeTransforms[i] = lightView * lightProj * T;

			XMFLOAT4X4 viewProj;
			XMStoreFloat4x4(&viewProj, XMMatrixTranspose(XMMatrixMultiply(lightView, lightProj)));
			m_PassBuffer->LoadData(context, viewProj);

			auto shadowTexDesc = m_ShadowMaps[i]->GetD3D12Resource()->GetDesc();
			D3D12_VIEWPORT viewport = { 0.0f, 0.0f, shadowTexDesc.Width, shadowTexDesc.Height, 0.0f, 1.0f };
			D3D12_RECT scissorRect = { 0.0f, 0.0f, shadowTexDesc.Width, shadowTexDesc.Height };

			commandContext->GetCmdList()->RSSetViewports(1, &viewport);
			commandContext->GetCmdList()->RSSetScissorRects(1, &scissorRect);

			commandContext->SetRenderTargets(0, nullptr, false, &m_ShadowMaps[i]->GetDSVView()->GetCpuHandle());

			commandContext->GetCmdList()->ClearDepthStencilView(m_ShadowMaps[i]->GetDSVView()->GetCpuHandle(),
				D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

			m_Pso.CommitPso(context);

			for (uint32 i = 0; i < mesh->GetMeshCount(); i++)
			{
				struct ObjData
				{
					XMFLOAT4X4 World;
				} objData;

				XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMMatrixScaling(20, 20, 20))); // TODO: scaling

				m_ObjBuffer->LoadData(context, objData);

				m_Pso.CommitBinder(context, m_Binder.get());
				commandContext->GetCmdList()->IASetIndexBuffer(&mesh->GetIndexBuffer(i)->GetView());
				commandContext->GetCmdList()->IASetVertexBuffers(0, 1, &mesh->GetVertexBuffer(i)->GetView());
				commandContext->GetCmdList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				commandContext->GetCmdList()->DrawIndexedInstanced(mesh->GetIndexCount(i), 1, 0, 0, 0);
			}

			commandContext->TransitionResource(m_ShadowMaps[i].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		commandContext->FlushResourceBarriers();

		for (int i = 0; i < m_CascadeCount; i++)
		{
			commandContext->TransitionResource(m_ShadowMaps[i].get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
	}

	XMMATRIX CSMRendering::CalculateLightView(Light* light)
	{
		XMVECTOR lightPos = XMLoadFloat3(&light->Position);
		XMVECTOR lightDir = XMLoadFloat3(&light->Direction);
		XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		if (XMVector3NearEqual(XMLoadFloat3(&light->Direction), upVector, XMVectorReplicate(1e-4f)) ||
			XMVector3NearEqual(XMLoadFloat3(&light->Direction), XMVectorNegate(upVector), XMVectorReplicate(1e-4f)))
		{
			upVector = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		}

		return XMMatrixLookAtLH(lightPos, XMVectorAdd(lightPos, lightDir), upVector);
	}

	XMMATRIX CSMRendering::CalculateCascadeProjection(float nearDist, float farDist, Camera* camera, XMMATRIX lightView)
	{
		float nx1 = nearDist * tan(camera->GetFovX() / 2.0);
		float fx1 = farDist * tan(camera->GetFovX() / 2.0);
		float ny1 = nearDist * tan(camera->GetFovY() / 2.0);
		float fy1 = farDist * tan(camera->GetFovY() / 2.0);

		XMVECTOR fPoses[8] =
		{
			{ +nx1, +ny1, nearDist, 1 },
			{ -nx1, +ny1, nearDist, 1 },
			{ +nx1, -ny1, nearDist, 1 },
			{ -nx1, -ny1, nearDist, 1 },
			{ +fx1, +fy1, farDist,  1 },
			{ -fx1, +fy1, farDist,  1 },
			{ +fx1, -fy1, farDist,  1 },
			{ -fx1, -fy1, farDist,  1 }
		};

		for (size_t i = 0; i < 8; i++)
		{
			fPoses[i] = DirectX::XMVector3TransformCoord(fPoses[i], DirectX::XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetViewMatrix())));
			fPoses[i] = DirectX::XMVector3TransformCoord(fPoses[i], lightView);
		}

		XMFLOAT3 minCorner;
		XMFLOAT3 maxCorner;

		XMStoreFloat3(&minCorner, fPoses[0]);
		XMStoreFloat3(&maxCorner, fPoses[0]);

		for (size_t i = 1; i < 8; i++)
		{
			DirectX::XMFLOAT3 cornerPos;
			DirectX::XMStoreFloat3(&cornerPos, fPoses[i]);

			if (cornerPos.x < minCorner.x) minCorner.x = cornerPos.x;
			if (cornerPos.y < minCorner.y) minCorner.y = cornerPos.y;
			if (cornerPos.z < minCorner.z) minCorner.z = cornerPos.z;

			if (cornerPos.x > maxCorner.x) maxCorner.x = cornerPos.x;
			if (cornerPos.y > maxCorner.y) maxCorner.y = cornerPos.y;
			if (cornerPos.z > maxCorner.z) maxCorner.z = cornerPos.z;
		}

		float l = minCorner.x;
		float b = minCorner.y;
		float n = minCorner.z - ShadowDistance;
		float r = maxCorner.x;
		float t = maxCorner.y;
		float f = maxCorner.z + ShadowDistance;

		return DirectX::XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
	}
}