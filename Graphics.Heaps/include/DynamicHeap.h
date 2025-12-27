#pragma once
#include "framework.h"
#include "IRenderDeviceD3D12.h"

#include <MemoryAllocatorT.h>
#include <map>

namespace EduEngine
{
	struct GRAPHICS_HEAPS_API DynamicHeapAllocation
	{
	public:
		DynamicHeapAllocation() = default;

		DynamicHeapAllocation(ID3D12Resource* pBuff, uint64 offset, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, void* cpuAddress) :
			pBuffer(pBuff),
			Offset(offset),
			GpuAddress(gpuAddress),
			CpuAddress(cpuAddress)
		{ }

		ID3D12Resource* GetResource() const { return pBuffer; }
		uint64 GetOffset() const { return Offset; }
		D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return GpuAddress; }
		void* GetCpuAddress() const { return CpuAddress; }

	private:
		ID3D12Resource* pBuffer = nullptr;
		uint64 Offset = 0;
		D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;
		void* CpuAddress = 0;
	};

	class GRAPHICS_HEAPS_API DynamicHeapPage
	{
	public:
		DynamicHeapPage(IRenderDeviceD3D12* device, uint64 size);

		DynamicHeapPage(const DynamicHeapPage& rhs) = delete;
		DynamicHeapPage& operator = (const DynamicHeapPage& rhs) = delete;

		DynamicHeapPage(DynamicHeapPage&& lhs) noexcept;
		DynamicHeapPage& operator = (DynamicHeapPage&& lhs) noexcept;

		D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress(uint64 offset = 0) const
		{
			return m_GpuVirtualAddress + offset;
		}

		void* GetCpuVirtualAddress(uint64 offset = 0) const
		{
			return static_cast<uint8*>(m_CpuVirtualAddress) + offset;
		}

		ID3D12Resource* GetResource() const
		{
			return m_pBuffer.Get();
		}

		uint64 GetSize() const
		{
			return m_Size;
		}

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_pBuffer;

		D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
		void* m_CpuVirtualAddress;
		uint64 m_Size;
	};

	class GRAPHICS_HEAPS_API DynamicHeapManager
	{
	public:
		DynamicHeapManager(IRenderDeviceD3D12* device, uint32 numReservePages, uint64 pageSize);
		~DynamicHeapManager();

		DynamicHeapManager(const DynamicHeapManager&) = delete;
		DynamicHeapManager& operator = (const DynamicHeapManager&) = delete;
		DynamicHeapManager(DynamicHeapManager&&) = delete;
		DynamicHeapManager& operator = (DynamicHeapManager&&) = delete;

		DynamicHeapPage Allocate(uint64 sizeInBytes);

		void ReleasePages(std::vector<DynamicHeapPage>& usedPages, QueueMask queueMask);
		void Destroy();

	private:
		using PagesAlloc = MemoryAllocator::MemoryAllocatorT<std::pair<const uint64, DynamicHeapPage>>;
		std::multimap<uint64, DynamicHeapPage, std::less<uint64>, PagesAlloc> m_AvailablePages;
		std::mutex m_AllocMutex;
		IRenderDeviceD3D12* m_Device;
		uint64 m_PageSize;

		friend class StaleDynamicPage;
	};

	class GRAPHICS_HEAPS_API DynamicHeap
	{
	public:
		DynamicHeap(DynamicHeapManager& manager);
		~DynamicHeap();

		DynamicHeapAllocation Allocate(uint64 sizeInBytes, uint64 alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
		
		void ReleasePages(QueueMask queueMask);

	private:
		std::vector<DynamicHeapPage> m_UsedPages;
		uint64 m_CurrentOffset = 0;

		DynamicHeapManager& m_Manager;
	};
}