#include "ResourceViewD3D12.h"

namespace EduEngine
{
	void ResourceViewD3D12::CreateUAV(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, false));
		m_Device->GetD3D12Device()->CreateUnorderedAccessView(m_d3d12Resource.Get(), nullptr, uavDesc, allocation.GetCpuHandle());
		m_UavView = std::make_unique<ResourceHeapView>(this, std::move(allocation), false);
	}

	void ResourceViewD3D12::CreateSRV(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, onCpu));
		m_Device->GetD3D12Device()->CreateShaderResourceView(m_d3d12Resource.Get(), srvDesc, allocation.GetCpuHandle());
		m_SrvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	void ResourceViewD3D12::CreateRTV(const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, onCpu));
		m_Device->GetD3D12Device()->CreateRenderTargetView(m_d3d12Resource.Get(), rtvDesc, allocation.GetCpuHandle());
		m_RtvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	void ResourceViewD3D12::CreateDSV(const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, onCpu));
		m_Device->GetD3D12Device()->CreateDepthStencilView(m_d3d12Resource.Get(), dsvDesc, allocation.GetCpuHandle());
		m_DsvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	void ResourceViewD3D12::CreateSampler(const D3D12_SAMPLER_DESC* samDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, onCpu));
		m_Device->GetD3D12Device()->CreateSampler(samDesc, allocation.GetCpuHandle());
		m_Sampler = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	DescriptorHeapAllocation ResourceViewD3D12::Allocate(const D3D12_DESCRIPTOR_HEAP_TYPE& type, bool onCpu)
	{
		return onCpu ?
			m_Device->AllocateCPUDescriptor(m_QueueId, type, 1) :
			m_Device->AllocateGPUDescriptor(m_QueueId, type, 1);
	}
}