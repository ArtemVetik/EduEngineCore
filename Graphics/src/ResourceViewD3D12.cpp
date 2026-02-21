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

	void ResourceViewD3D12::CreateUAV_Array(D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, uint32 mipCount, uint32 depth, bool onCpu)
	{
		auto resDesc = m_d3d12Resource->GetDesc();
		uint8 resMipCount = resDesc.MipLevels;
		uint8 resDepth = resDesc.DepthOrArraySize;

		if (mipCount > resMipCount || depth > resDepth)
		{
			LOG_ERROR("Failed to create UAV Array! Passed Mip/Depth = ", mipCount, "/", depth, ". Max resource Mip/Depth = ", resMipCount, "/", resDepth);
			return;
		}

		VERIFY_EXPR(resMipCount > 0 && resMipCount < UINT8_MAX, "");
		VERIFY_EXPR(resDepth > 0 && resDepth < UINT8_MAX, "");

		if (mipCount == 0) mipCount = resMipCount;
		if (depth == 0) depth = resDepth;

		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, mipCount * depth, onCpu));

		for (uint8 mip = 0; mip < mipCount; mip++)
		{
			if (mipCount > 1 && uavDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
			{
				ASSERT_FAILED("It is not possible to set MipSlice for UAV with ViewDimension = [", uavDesc.ViewDimension, "]");
				return;
			}

			uavDesc.Texture2DArray.MipSlice = mip;

			for (uint8 arraySlice = 0; arraySlice < depth; arraySlice++)
			{
				if (depth > 1 && uavDesc.ViewDimension != D3D12_UAV_DIMENSION_TEXTURE1DARRAY &&
					uavDesc.ViewDimension != D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
				{
					ASSERT_FAILED("It is not possible to set FirstArraySlice for UAV with ViewDimension = [", uavDesc.ViewDimension, "]");
					return;
				}

				uavDesc.Texture2DArray.FirstArraySlice = arraySlice;

				m_Device->GetD3D12Device()->CreateUnorderedAccessView(m_d3d12Resource.Get(), nullptr, &uavDesc, allocation.GetCpuHandle(mip * depth + arraySlice));
			}
		}

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

	void ResourceViewD3D12::CreateRTV_Array(D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, uint32 mipCount, uint32 depth)
	{
		auto resDesc = m_d3d12Resource->GetDesc();
		uint8 resMipCount = resDesc.MipLevels;
		uint8 resDepth = resDesc.DepthOrArraySize;

		VERIFY_EXPR(resMipCount > 0 && resMipCount < UINT8_MAX, "");
		VERIFY_EXPR(resDepth > 0 && resDepth < UINT8_MAX, "");
		VERIFY_EXPR(rtvDesc.Format == m_d3d12Resource->GetDesc().Format, "");

		if (mipCount > resMipCount || depth > resDepth)
		{
			LOG_ERROR("Failed to create RTV Array! Passed Mip/Depth = ", mipCount, "/", depth, ". Max resource Mip/Depth = ", resMipCount, "/", resDepth);
			return;
		}

		if (mipCount == 0) mipCount = resMipCount;
		if (depth == 0) depth = resDepth;

		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, mipCount * depth, true));

		for (uint8 mip = 0; mip < mipCount; mip++)
		{
			if (mipCount > 1 && (rtvDesc.ViewDimension == D3D12_RTV_DIMENSION_BUFFER ||
				rtvDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMS ||
				rtvDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY))
			{
				ASSERT_FAILED("It is not possible to set MipSlice for RTV with ViewDimension = [", rtvDesc.ViewDimension, "]");
				return;
			}

			rtvDesc.Texture2DArray.MipSlice = mip;

			for (uint8 arraySlice = 0; arraySlice < depth; arraySlice++)
			{
				if (depth > 1 && rtvDesc.ViewDimension != D3D12_RTV_DIMENSION_TEXTURE1DARRAY &&
					rtvDesc.ViewDimension != D3D12_RTV_DIMENSION_TEXTURE2DARRAY &&
					rtvDesc.ViewDimension != D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
				{
					ASSERT_FAILED("It is not possible to set FirstArraySlice for RTV with ViewDimension = [", rtvDesc.ViewDimension, "]");
					return;
				}

				if (rtvDesc.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
					rtvDesc.Texture2DMSArray.FirstArraySlice = arraySlice;
				else
					rtvDesc.Texture2DArray.FirstArraySlice = arraySlice;

				m_Device->GetD3D12Device()->CreateRenderTargetView(m_d3d12Resource.Get(), &rtvDesc, allocation.GetCpuHandle(mip * depth + arraySlice));
			}
		}

		m_RtvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), true);
	}

	void ResourceViewD3D12::CreateDSV(const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc)
	{
		DescriptorHeapAllocation allocation = std::move(Allocate(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, true));
		m_Device->GetD3D12Device()->CreateDepthStencilView(m_d3d12Resource.Get(), dsvDesc, allocation.GetCpuHandle());
		m_DsvView = std::make_unique<ResourceHeapView>(this, std::move(allocation), true);
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
			m_Device->AllocateCPUDescriptor(type, count) :
			m_Device->AllocateGPUDescriptor(type, count);
	}
}