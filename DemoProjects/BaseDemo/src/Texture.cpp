#include "Texture.h"

namespace EduEngine
{
	Texture::Texture() :
		m_Device(nullptr),
		m_Texture(nullptr),
		m_GpuAllocation{}
	{ }

	Texture::~Texture()
	{
		m_Texture.reset();
	}

	void Texture::Load(const wchar_t* filePath,
					   RenderDeviceD3D12* device,
					   DeviceContext* context,
					   D3D12_SHADER_RESOURCE_VIEW_DESC* overrideDesc,
					   wchar_t* name)
	{
		m_Device = device;
		m_Texture = std::make_shared<TextureD3D12>(m_Device, context, std::wstring(filePath), QueueId::Direct);

		auto texDesc = m_Texture->GetD3D12Resource()->GetDesc();
		bool cubeMap = texDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && texDesc.DepthOrArraySize == 6;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		if (overrideDesc)
		{
			srvDesc = *overrideDesc;
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

		m_Texture->CreateSRV(&srvDesc, true);
		if (name)
			m_Texture->SetName(name);

		m_GpuAllocation.Reset();
	}

	void* Texture::GetGPUPtr()
	{
		if (m_GpuAllocation.IsNull())
		{
			m_GpuAllocation = m_Device->AllocateGPUDescriptor(QueueId::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
			m_Device->GetD3D12Device()->CopyDescriptorsSimple(1, m_GpuAllocation.GetCpuHandle(), m_Texture->GetSRVView()->GetCpuHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		if (m_Texture.get())
			return reinterpret_cast<void*>(m_GpuAllocation.GetGpuHandle().ptr);

		return nullptr;
	}
}