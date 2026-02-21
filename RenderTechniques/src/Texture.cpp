#include "Texture.h"

namespace EduEngine
{
	Texture::Texture(RenderDeviceD3D12* device, std::shared_ptr<TextureD3D12> texture, const TextureLoadDesc& loadDesc) :
		Texture(device, loadDesc)
	{
		m_Texture = texture;
		SetupTexture();
	}

	Texture::Texture(RenderDeviceD3D12* device, const TextureLoadDesc& loadDesc) :
		m_Device(device),
		m_Texture(nullptr),
		m_GpuAllocation{},
		m_LoadDesc(loadDesc)
	{ }

	Texture::~Texture()
	{
		m_Texture.reset();
	}

	void Texture::Load(const wchar_t* filePath,
					   DeviceContext* context,
					   const wchar_t* name)
	{

		m_Texture = std::make_shared<TextureD3D12>(m_Device, context, std::wstring(filePath), QueueId::Direct);
		if (name)
			m_Texture->SetName(name);

		m_GpuAllocation.Reset();
		SetupTexture();
	}

	void* Texture::GetGPUPtr()
	{
		if (!m_Texture.get())
		{
			LOG_ERROR("Texture wasn't loaded");
			return nullptr;
		}

		VERIFY_EXPR(m_LoadDesc.Flags & TextureLoadDesc::CREATE_SRV, "");

		if (m_LoadDesc.OnCPU == false)
		{
			return reinterpret_cast<void*>(m_Texture->GetSRVView()->GetGpuHandle().ptr);
		}

		if (m_GpuAllocation.IsNull())
		{
			m_GpuAllocation = m_Device->AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			m_Device->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuAllocation.GetCpuHandle(), m_Texture->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		if (m_Texture.get())
			return reinterpret_cast<void*>(m_GpuAllocation.GetGpuHandle().ptr);

		return nullptr;
	}

	void Texture::SetupTexture()
	{
		if ((m_LoadDesc.Flags & TextureLoadDesc::CREATE_SRV) == 0)
			return;

		auto texDesc = m_Texture->GetD3D12Resource()->GetDesc();
		bool cubeMap = texDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && texDesc.DepthOrArraySize == 6;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		if (m_LoadDesc.OverrideDesc)
		{
			srvDesc = *m_LoadDesc.OverrideDesc;
			srvDesc.Format = m_Texture->GetD3D12Resource()->GetDesc().Format;
		}
		else
		{
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = m_Texture->GetD3D12Resource()->GetDesc().Format;
			srvDesc.ViewDimension = cubeMap ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;

			if (cubeMap)
			{
				srvDesc.TextureCube.MostDetailedMip = 0;
				srvDesc.TextureCube.MipLevels = m_Texture->GetD3D12Resource()->GetDesc().MipLevels;
				srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
			}
			else
			{
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = m_Texture->GetD3D12Resource()->GetDesc().MipLevels;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			}
		}
		
		m_Texture->CreateSRV(&srvDesc, m_LoadDesc.OnCPU);
	}
}