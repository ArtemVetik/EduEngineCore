#include "DynamicHeap.h"
#include "ReleaseResourceWrapper.h"

#include <Asserts.h>

namespace EduEngine
{
	DynamicHeapPage::DynamicHeapPage(IRenderDeviceD3D12* device, uint64 size) :
		m_Size(size),
		m_GpuVirtualAddress(0),
		m_CpuVirtualAddress(nullptr)
	{
		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC resourceDesc;
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resourceDesc.Alignment = 0;
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		D3D12_RESOURCE_STATES defaultUsage;
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		defaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
		resourceDesc.Width = size;

		HRESULT hr = device->GetD3D12Device()->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			defaultUsage,
			nullptr,
			IID_PPV_ARGS(&m_pBuffer)
		);

		if (FAILED(hr))
		{
			ASSERT_FAILED("Failed to create DynamicHeapPage");
			return;
		}

		m_pBuffer->SetName(L"Dynamic Heap Page");

		m_GpuVirtualAddress = m_pBuffer->GetGPUVirtualAddress();

		m_pBuffer->Map(0, nullptr, &m_CpuVirtualAddress);
	}

	DynamicHeapPage::DynamicHeapPage(DynamicHeapPage&& lhs) noexcept
	{
		m_pBuffer = std::move(lhs.m_pBuffer);
		m_GpuVirtualAddress = lhs.m_GpuVirtualAddress;
		m_CpuVirtualAddress = lhs.m_CpuVirtualAddress;
		m_Size = lhs.m_Size;

		lhs.m_GpuVirtualAddress = 0;
		lhs.m_CpuVirtualAddress = nullptr;
		lhs.m_Size = 0;
	}

	DynamicHeapPage& DynamicHeapPage::operator=(DynamicHeapPage&& lhs) noexcept
	{
		if (this != &lhs)
		{
			std::swap(m_pBuffer, lhs.m_pBuffer);
			std::swap(m_GpuVirtualAddress, lhs.m_GpuVirtualAddress);
			std::swap(m_CpuVirtualAddress, lhs.m_CpuVirtualAddress);
			std::swap(m_Size, lhs.m_Size);
		}

		return *this;
	}

	DynamicHeapManager::DynamicHeapManager(IRenderDeviceD3D12* device, uint32 numReservePages, uint64 pageSize) :
		m_Device(device),
		m_PageSize(pageSize)
	{
		for (uint32 i = 0; i < numReservePages; i++)
		{
			m_AvailablePages.emplace(m_PageSize, std::move(DynamicHeapPage(m_Device, m_PageSize)));
		}
	}

	DynamicHeapManager::~DynamicHeapManager()
	{
		VERIFY_EXPR(m_AvailablePages.empty(), "DynamicHeapManager is not fully released");
	}

	DynamicHeapPage DynamicHeapManager::Allocate(uint64 sizeInBytes)
	{
		std::lock_guard<std::mutex> LockGuard(m_AllocMutex);

		auto pageIt = m_AvailablePages.lower_bound(sizeInBytes);

		if (pageIt != m_AvailablePages.end())
		{
			auto page = std::move(pageIt->second);
			m_AvailablePages.erase(pageIt);

			return std::move(page);
		}

		return std::move(DynamicHeapPage(m_Device, std::max(m_PageSize, sizeInBytes)));
	}

	void DynamicHeapManager::ReleasePages(std::vector<DynamicHeapPage>& usedPages, QueueID queueId)
	{
		for (auto& page : usedPages)
		{
			StaleDynamicPage stalePage(std::move(page), this);

			ReleaseResourceWrapper staleWrapper = {};
			staleWrapper.AddStaleDynamicPage(std::move(stalePage));

			m_Device->SafeReleaseObject(queueId, std::move(staleWrapper));
		}

		usedPages.clear();
	}

	void DynamicHeapManager::Destroy()
	{
		m_AvailablePages.clear();
	}

	DynamicHeap::DynamicHeap(DynamicHeapManager& manager) :
		m_Manager(manager),
		m_CurrentOffset(0)
	{
	}

	DynamicHeap::~DynamicHeap()
	{
		VERIFY_EXPR(m_UsedPages.empty(), "DynamicHeap is not fully released");
	}

	DynamicHeapAllocation DynamicHeap::Allocate(uint64 sizeInBytes, uint64 alignment)
	{
		VERIFY_EXPR(alignment > 0, "Alignment must be greater than zero");

		const uint64 alignmentMask = alignment - 1;
		VERIFY_EXPR((alignmentMask & alignment) == 0, "Alignment (", alignment, ") must be power of two");

		uint64 alignedOffset = (m_CurrentOffset + alignmentMask) & ~alignmentMask;

		if (m_UsedPages.empty() || alignedOffset + sizeInBytes > m_UsedPages.back().GetSize())
		{
			m_UsedPages.push_back(std::move(m_Manager.Allocate(sizeInBytes)));
			alignedOffset = 0;
		}

		m_CurrentOffset = alignedOffset + sizeInBytes;

		auto& back = m_UsedPages.back();

		return DynamicHeapAllocation(back.GetResource(), alignedOffset, back.GetGpuVirtualAddress(alignedOffset), back.GetCpuVirtualAddress(alignedOffset));
	}

	void DynamicHeap::ReleasePages(QueueID queueId)
	{
		m_Manager.ReleasePages(m_UsedPages, queueId);

		m_CurrentOffset = 0;
	}
}