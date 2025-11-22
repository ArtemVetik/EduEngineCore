#include "ResourceViewD3D12.h"

#include <Asserts.h>

namespace EduEngine
{
	void ResourceViewD3D12::CreateCBV()
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false));

		D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
		desc.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
		desc.SizeInBytes = m_d3d12Resource->GetDesc().Width;

		m_Device->GetD3D12Device()->CreateConstantBufferView(&desc, allocation.GetCpuHandle());
		m_CbvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), false);
	}

	void ResourceViewD3D12::CreateUAV(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, onCpu));
		m_Device->GetD3D12Device()->CreateUnorderedAccessView(m_d3d12Resource.Get(), nullptr, uavDesc, allocation.GetCpuHandle());
		m_UavView = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	void ResourceViewD3D12::CreateSRV(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, onCpu));
		m_Device->GetD3D12Device()->CreateShaderResourceView(m_d3d12Resource.Get(), srvDesc, allocation.GetCpuHandle());
		m_SrvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	void ResourceViewD3D12::CreateRTV(const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, true));
		m_Device->GetD3D12Device()->CreateRenderTargetView(m_d3d12Resource.Get(), rtvDesc, allocation.GetCpuHandle());
		m_RtvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), true);
	}

	void ResourceViewD3D12::CreateRTV2DArray()
	{
		auto resDesc = m_d3d12Resource->GetDesc();
		uint8 mipCount = resDesc.MipLevels;
		uint8 depth = resDesc.DepthOrArraySize;
		
		VERIFY_EXPR(mipCount > 0 && mipCount < UINT8_MAX, "");
		VERIFY_EXPR(depth > 0 && depth < UINT8_MAX, "");

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = m_d3d12Resource->GetDesc().Format;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY; // So far, only D3D12_RTV_DIMENSION_TEXTURE2DARRAY is supported.
		rtvDesc.Texture2DArray.ArraySize = 1;

		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, mipCount * depth, true));

		for (uint8 mip = 0; mip < mipCount; mip++)
		{
			rtvDesc.Texture2DArray.MipSlice = mip;
			for (uint8 arraySlice = 0; arraySlice < depth; arraySlice++)
			{
				rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;
				m_Device->GetD3D12Device()->CreateRenderTargetView(m_d3d12Resource.Get(), &rtvDesc, allocation.GetCpuHandle(mip * depth + arraySlice));
			}
		}

		m_RtvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), true);
	}

	void ResourceViewD3D12::CreateDSV(const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, onCpu));
		m_Device->GetD3D12Device()->CreateDepthStencilView(m_d3d12Resource.Get(), dsvDesc, allocation.GetCpuHandle());
		m_DsvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	void ResourceViewD3D12::CreateSampler(const D3D12_SAMPLER_DESC* samDesc, bool onCpu)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, onCpu));
		m_Device->GetD3D12Device()->CreateSampler(samDesc, allocation.GetCpuHandle());
		m_Sampler = std::make_unique<ResourceHeapView>(this, std::move(allocation), onCpu);
	}

	DescriptorHeapAllocation ResourceViewD3D12::Allocate(const D3D12_DESCRIPTOR_HEAP_TYPE& type, size_t count, bool onCpu)
	{
		return onCpu ?
			m_Device->AllocateCPUDescriptor(m_QueueId, type, count) :
			m_Device->AllocateGPUDescriptor(m_QueueId, type, count);
	}
}