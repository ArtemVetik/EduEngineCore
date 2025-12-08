#pragma once
#include "framework.h"
#include "ResourceD3D12.h"

namespace EduEngine
{
	class GRAPHICS_API ResourceHeapView
	{
	private:
		DescriptorHeapAllocation m_Allocation;
		bool m_OnCpu;

	public:
		ResourceHeapView(ResourceD3D12* parent, DescriptorHeapAllocation&& allocation, bool onCpu) :
			m_Allocation(std::move(allocation)),
			m_OnCpu(onCpu)
		{ }

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32 offset = 0) const { return m_Allocation.GetCpuHandle(offset); }

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32 offset = 0) const { return m_Allocation.GetGpuHandle(offset); }

		bool OnCpu() const { return m_OnCpu; }
	};

	class GRAPHICS_API ResourceViewD3D12 : public ResourceD3D12
	{
	public:
		ResourceViewD3D12(RenderDeviceD3D12* pDevice, QueueMask queueMask) :
			ResourceD3D12(pDevice, queueMask)
		{ }

		ResourceViewD3D12(RenderDeviceD3D12* pDevice, Microsoft::WRL::ComPtr<ID3D12Resource>& resource, QueueMask queueMask) :
			ResourceD3D12(pDevice, resource, queueMask)
		{ }

		void CreateCBV();
		void CreateUAV(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, bool onCpu = true);
		void CreateUAV_Array(D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc, uint32 mipCount = 0, uint32 depth = 0, bool onCpu = true);
		void CreateSRV(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, bool onCpu = true);
		void CreateRTV(const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc);
		void CreateRTV_Array(D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc, uint32 mipCount = 0, uint32 depth = 0);
		void CreateDSV(const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc);
		void CreateSampler(const D3D12_SAMPLER_DESC* desc, bool onCpu = true);

		ResourceHeapView* GetCBVView() const { return m_CbvView.get(); }
		ResourceHeapView* GetUAVView() const { return m_UavView.get(); }
		ResourceHeapView* GetSRVView() const { return m_SrvView.get(); }
		ResourceHeapView* GetRTVView() const { return m_RtvView.get(); }
		ResourceHeapView* GetDSVView() const { return m_DsvView.get(); }
		ResourceHeapView* GetSampler() const { return m_Sampler.get(); }

	private:
		DescriptorHeapAllocation Allocate(const D3D12_DESCRIPTOR_HEAP_TYPE& type, size_t count, bool onCpu);

	private: // TODO: create ALLOWED_VIEWS_FLAG to reduce class size from unused pointers
		std::unique_ptr<ResourceHeapView> m_CbvView;
		std::unique_ptr<ResourceHeapView> m_UavView;
		std::unique_ptr<ResourceHeapView> m_SrvView;
		std::unique_ptr<ResourceHeapView> m_RtvView;
		std::unique_ptr<ResourceHeapView> m_DsvView;
		std::unique_ptr<ResourceHeapView> m_Sampler;
	};
}