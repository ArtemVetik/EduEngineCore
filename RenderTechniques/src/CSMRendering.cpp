#include "CSMRendering.h"

#include <DynamicUploadBuffer.h>
#include <Asserts.h>

namespace EduEngine
{
	CSMRendering::CSMRendering(RenderDeviceD3D12* device, DeviceContext* context, const Settings& settings) :
		m_Device(device),
		m_Settings(settings)
	{
		BuildShadowMaps(context);

		//
		// Build PSO
		//

		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;
		sDesc.ResourceNum = 0;

		std::shared_ptr<ShaderD3D12> VS = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ShadowPass.hlsl", L"VS", L"vs_6_0", nullptr, sDesc);

		D3D12_INPUT_ELEMENT_DESC inputLayout[] // TODO: take into account only POSITION
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	  0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_RASTERIZER_DESC rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rast.DepthBias = -1;
		rast.DepthBiasClamp = 0.0f;
		rast.SlopeScaledDepthBias = -2.5f;

		D3D12_DEPTH_STENCIL_DESC dss = {};
		dss.DepthEnable = true;
		dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		dss.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;

		m_Pso.SetInputLayout({ inputLayout, _countof(inputLayout) });
		m_Pso.SetRasterizerState(rast);
		m_Pso.SetDepthStencilState(dss);
		m_Pso.SetShader(VS);
		m_Pso.Build(device);
		m_Pso.SetName(L"ShadowMapPSO");

		m_ObjBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);

		m_Binder = m_Pso.CreateShaderBinder();
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPerObject", m_ObjBuffer);
		m_Binder->BindDynamicResource(EDU_SHADER_TYPE_VERTEX, "cbPass", m_PassBuffer);
	}

	void CSMRendering::Update(DeviceContext* context, Camera* camera, Light* light)
	{
		//
		// Update:
		//	- m_CascadeTransforms
		//	- m_CascadeSpheres
		//	- m_CascadeSphereRad2 
		//

		XMMATRIX invCamView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&camera->GetViewMatrix()));
		XMMATRIX lightView = CalculateLightView(light);
		XMVECTOR lightDir = XMVector3Normalize(XMLoadFloat3(&light->Direction));

		XMStoreFloat3(&m_LightDirection, XMVectorNegate(lightDir));

		float cascadeNear = 0.0f;

		for (int i = 0; i < m_Settings.CascadesCount; i++)
		{
			float cascadeFar = m_Settings.CSMSplits[i] * m_Settings.ShadowDistance;

			// TODO: cache local bounding sphere (no need to calculate it every frame)
			camera->CalculateLocalBoundingSphere(cascadeNear, cascadeFar, m_CascadeSpheres[i]);
			cascadeNear = cascadeFar;
			
			float sphereRadius = m_CascadeSpheres[i].w;

			XMVECTOR worldBoundingSphere = XMVector3TransformCoord(XMLoadFloat4(&m_CascadeSpheres[i]), invCamView);
			XMStoreFloat4(&m_CascadeSpheres[i], worldBoundingSphere);
			m_CascadeSpheres[i].w = sphereRadius;

			*(&m_CascadeSphereRad2.x + i) = sphereRadius * sphereRadius;

			XMMATRIX lightProj = CalculateCascadeProjection(lightView, m_CascadeSpheres[i], m_Settings.CSMSizes[i]);
			
			// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
			DirectX::XMMATRIX T(
				0.5f, 0.0f, 0.0f, 0.0f,
				0.0f, -0.5f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.5f, 0.5f, 0.0f, 1.0f);

			m_CascadeTransforms[i] = lightView * lightProj * T;

			XMStoreFloat4x4(&m_ViewProj[i], XMMatrixTranspose(XMMatrixMultiply(lightView, lightProj)));
		}
	}

	void CSMRendering::Render(DeviceContext* context, RenderObject* objects, uint32 objectsNum)
	{
		auto* commandContext = context->GetCommandCtx();

		for (int cascadeIdx = 0; cascadeIdx < m_Settings.CascadesCount; cascadeIdx++)
		{
			auto shadowTexDesc = m_ShadowMaps[cascadeIdx]->GetD3D12Resource()->GetDesc();
			D3D12_VIEWPORT viewport = { 0.0f, 0.0f, shadowTexDesc.Width, shadowTexDesc.Height, 0.0f, 1.0f };
			D3D12_RECT scissorRect = { 0.0f, 0.0f, shadowTexDesc.Width, shadowTexDesc.Height };

			commandContext->GetCmdList()->RSSetViewports(1, &viewport);
			commandContext->GetCmdList()->RSSetScissorRects(1, &scissorRect);

			commandContext->SetRenderTargets(0, nullptr, false, &m_ShadowMaps[cascadeIdx]->GetDSVView()->GetCpuHandle());

			commandContext->GetCmdList()->ClearDepthStencilView(m_ShadowMaps[cascadeIdx]->GetDSVView()->GetCpuHandle(),
				D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

			struct PassData
			{
				XMFLOAT4X4 ViewProj;
				XMFLOAT3 LigthDirection;
				UINT Padding;
				XMFLOAT4 ShadowBias;
			} passData;

			passData.ViewProj = m_ViewProj[cascadeIdx];
			passData.LigthDirection = m_LightDirection;
			passData.ShadowBias = XMFLOAT4(-m_Settings.ShadowBias.x, -m_Settings.ShadowBias.y, 0.0f, 0.0f);

			m_PassBuffer->LoadData(context, passData);

			m_Pso.BeginPSO(context);

			for (uint32 obj = 0; obj < objectsNum; obj++)
			{
				for (uint32 mIdx = 0; mIdx < objects[obj].Mesh->GetMeshCount(); mIdx++)
				{
					struct ObjData
					{
						XMFLOAT4X4 World;
					} objData;

					XMStoreFloat4x4(&objData.World, XMMatrixTranspose(objects[obj].World));

					m_ObjBuffer->LoadData(context, objData);

					m_Pso.CommitResources(context, m_Binder.get());
					commandContext->GetCmdList()->IASetIndexBuffer(&objects[obj].Mesh->GetIndexBuffer(mIdx)->GetView());
					commandContext->GetCmdList()->IASetVertexBuffers(0, 1, &objects[obj].Mesh->GetVertexBuffer(mIdx)->GetView());
					commandContext->GetCmdList()->DrawIndexedInstanced(objects[obj].Mesh->GetIndexCount(mIdx), 1, 0, 0, 0);
				}
			}

			commandContext->TransitionResource(m_ShadowMaps[cascadeIdx].get(), D3D12_RESOURCE_STATE_GENERIC_READ);
		}

		commandContext->FlushResourceBarriers();

		for (int i = 0; i < m_Settings.CascadesCount; i++)
		{
			commandContext->TransitionResource(m_ShadowMaps[i].get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}
	}

	void CSMRendering::UpdateSettings(DeviceContext* context, Settings newSettings)
	{
		bool splitsChanged = memcmp(&newSettings.CSMSplits, m_Settings.CSMSplits, sizeof(float) * MAX_CASCADES);
		bool sizeChanged = memcmp(&newSettings.CSMSizes, m_Settings.CSMSizes, sizeof(XMFLOAT2) * MAX_CASCADES);

		if (newSettings.CascadesCount != m_Settings.CascadesCount || sizeChanged)
		{
			VERIFY_EXPR(newSettings.CascadesCount <= MAX_CASCADES && newSettings.CascadesCount > 0,
				"Settings.CascadesCount (", newSettings.CascadesCount,") > MAX_CASCADES (", MAX_CASCADES, ")");

			for (uint32 i = 0; i < m_Settings.CascadesCount - 1; i++)
			{
				VERIFY_EXPR(newSettings.CSMSplits[i] <= newSettings.CSMSplits[i + 1] && newSettings.CSMSplits[i] <= 1.0f,
					"Invalid CSMSplit (", newSettings.CSMSplits[i], ")");
			}

			newSettings.CSMSplits[newSettings.CascadesCount - 1] = 1.0f;
			m_Settings = newSettings;

			BuildShadowMaps(context);
		}
		else
		{
			m_Settings = newSettings;
		}
	}

	void CSMRendering::BuildShadowMaps(DeviceContext* context)
	{
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
		optClear.DepthStencil.Depth = 0.0f;
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

		for (size_t i = 0; i < m_Settings.CascadesCount; i++)
		{
			texDesc.Width = m_Settings.CSMSizes[i].x;
			texDesc.Height = m_Settings.CSMSizes[i].y;

			m_ShadowMaps[i] = std::make_unique<TextureD3D12>(m_Device, texDesc, &optClear, QueueId::Direct);
			m_ShadowMaps[i]->CreateDSV(&dsvDesc);

			context->GetCommandCtx()->TransitionResource(m_ShadowMaps[i].get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
		}

		for (size_t i = 0; i < m_Settings.CascadesCount; i++)
			m_ShadowMaps[i]->CreateSRV(&srvDesc, false);

		context->GetCommandCtx()->FlushResourceBarriers();
	}

	XMMATRIX CSMRendering::CalculateLightView(Light* light)
	{
		XMVECTOR lightPos = XMLoadFloat3(&light->Position);
		XMVECTOR lightDir = XMLoadFloat3(&light->Direction);
		XMVECTOR upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		if (XMVector3NearEqual(XMLoadFloat3(&light->Direction), upVector, g_XMEpsilon) ||
			XMVector3NearEqual(XMLoadFloat3(&light->Direction), XMVectorNegate(upVector), g_XMEpsilon))
		{
			upVector = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		}

		return XMMatrixLookAtLH(lightPos, XMVectorAdd(lightPos, lightDir), upVector);
	}

	XMMATRIX CSMRendering::CalculateCascadeProjection(XMMATRIX lightView, XMFLOAT4 boundingSphere, XMFLOAT2 csmSize)
	{
		float sphereRadius = boundingSphere.w;

		// Transform to light view space
		XMVECTOR sphereCenter = XMLoadFloat4(&boundingSphere);
		sphereCenter = XMVector3TransformCoord(sphereCenter, lightView);
		XMStoreFloat4(&boundingSphere, sphereCenter);

		// Snap center
		float orthoWidth = 2.0f * sphereRadius;
		float orthoHeight = 2.0f * sphereRadius;

		float texelSizeX = orthoWidth / csmSize.x;
		float texelSizeY = orthoHeight / csmSize.y;

		boundingSphere.x = floor(boundingSphere.x / texelSizeX) * texelSizeX;
		boundingSphere.y = floor(boundingSphere.y / texelSizeY) * texelSizeY;

		float l = boundingSphere.x - sphereRadius;
		float r = boundingSphere.x + sphereRadius;
		float b = boundingSphere.y - sphereRadius;
		float t = boundingSphere.y + sphereRadius;
		float n = boundingSphere.z - sphereRadius - m_Settings.ShadowDistance; // TODO: calc necessary near and far distance
		float f = boundingSphere.z + sphereRadius + m_Settings.ShadowDistance;

		return DirectX::XMMatrixOrthographicOffCenterLH(l, r, b, t, f, n);
	}
}