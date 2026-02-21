#include "ORMTextureGenerator.h"

#include <Asserts.h>
#include <SimpleMath.h>

namespace EduEngine
{
	ORMTextureGenerator::ORMTextureGenerator(RenderDeviceD3D12* device, QueueMask queueMask) :
		m_Device(device),
		m_QueueMask(queueMask)
	{
		m_PassBuffer = std::make_shared<DynamicUploadBuffer>(m_Device);
		
		m_PsoEntry.Name = "ORMTextureGen";
		m_PsoEntry.DependentParams = {};
		m_PsoEntry.BuildPsoFunc = [this]() { return BuildPSO(); };
		m_PsoEntry.OnPsoUpdated = [this]() 
			{
				m_Binder = m_PsoEntry.Pso->CreateShaderBinder();
				m_Binder->BindDynamicResource(EDU_SHADER_TYPE_COMPUTE, "cbPass", m_PassBuffer);
			};
		m_PsoEntry.Initialize();
	}

	std::unique_ptr<TextureD3D12> ORMTextureGenerator::Generate(DeviceContext* context,
																TextureD3D12* metalRoughness,
																TextureD3D12* ao)
	{
		D3D12_RESOURCE_DESC metalRoughDesc = metalRoughness->GetD3D12Resource()->GetDesc();
		D3D12_RESOURCE_DESC aoDesc = ao->GetD3D12Resource()->GetDesc();

		VERIFY_EXPR(metalRoughDesc.Width == aoDesc.Width,
			"The Width of the textures does not match. MetalRough(", metalRoughDesc.Width, "), AO(", aoDesc.Width, ")");
		VERIFY_EXPR(metalRoughDesc.Height == aoDesc.Height,
			"The Height of the textures does not match. MetalRough(", metalRoughDesc.Height, "), AO(", aoDesc.Height, ")");
		VERIFY_EXPR(metalRoughDesc.Format == aoDesc.Format,
			"The Format of the textures does not match. MetalRough(", metalRoughDesc.Format, "), AO(", aoDesc.Format, ")");
		VERIFY_EXPR(metalRoughDesc.MipLevels == aoDesc.MipLevels,
			"The MipLevels of the textures does not match. MetalRough(", metalRoughDesc.MipLevels, "), AO(", aoDesc.MipLevels, ")");

		D3D12_RESOURCE_DESC ormDesc = metalRoughDesc;
		ormDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: if BC1, BC3, BC5, BC7, set DXGI_FORMAT_R8G8B8A8_UNORM
		ormDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Format = ormDesc.Format;
		srvDesc.Texture2D.MipLevels = ormDesc.MipLevels;
		srvDesc.Texture2D.PlaneSlice = 0;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Format = ormDesc.Format;
		uavDesc.Texture2D.PlaneSlice = 0;
		uavDesc.Texture2D.MipSlice = 0;

		auto ormTexture = std::make_unique<TextureD3D12>(m_Device, ormDesc, nullptr, metalRoughness->GetQueueMask());
		ormTexture->CreateSRV(&srvDesc, false);
		ormTexture->CreateUAV_Array(uavDesc, 0, 0, false);

		srvDesc.Format = metalRoughDesc.Format;
		metalRoughness->CreateSRV(&srvDesc, false);
		ao->CreateSRV(&srvDesc, false);

		struct PassData
		{
			UINT MetalRoughnessTexIdx;
			UINT AOTexIdx;
			UINT OutORMTexIdx;
			UINT MipLevel;
			DirectX::XMUINT2 TexSize;
		} passData;

		for (uint32 i = 0; i < ormDesc.MipLevels; i++)
		{
			uint64 width = std::max((int)(ormDesc.Width >> i), 1);
			uint64 height = std::max((int)(ormDesc.Height >> i), 1);

			passData.MetalRoughnessTexIdx = metalRoughness->GetSRVView()->GetGpuHeapIndex();
			passData.AOTexIdx = ao->GetSRVView()->GetGpuHeapIndex();
			passData.OutORMTexIdx = ormTexture->GetUAVView()->GetGpuHeapIndex(i);
			passData.MipLevel = i;
			passData.TexSize = { (UINT)width, (UINT)height };

			m_PassBuffer->LoadData(context, passData);

			m_PsoEntry.Pso->CommitAll(context, m_Binder.get());
			context->GetCommandCtx()->GetCmdList()->Dispatch((UINT)std::ceil(width / 8.0f), (UINT)std::ceil(height / 8.0f), 1);
		}

		return ormTexture;
	}

	std::shared_ptr<ComputePipelineState> ORMTextureGenerator::BuildPSO()
	{
		ShaderDesc sDesc = {};
		sDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;
		sDesc.ResourceNum = 0;

		auto cs = std::make_shared<ShaderD3D12>(L"assets\\Shaders\\ORMTextureGen.hlsl", L"CSMain", L"cs_6_6", nullptr, sDesc);

		auto pso = std::make_shared<ComputePipelineState>(m_QueueMask);
		pso->SetShader(cs);
		pso->Build(m_Device);
		pso->SetName(L"PSO_ORMTextureGen");

		return pso;
	}
}