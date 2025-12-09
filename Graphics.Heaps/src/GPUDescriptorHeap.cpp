#include "pch.h"
#include "GPUDescriptorHeap.h"

namespace EduEngine
{
    GPUDescriptorHeap::GPUDescriptorHeap(IRenderDeviceD3D12&         device,
                                         uint32                      numDescriptorsInHeap,
                                         uint32                      numDynamicDescriptors,
                                         D3D12_DESCRIPTOR_HEAP_TYPE  type,
                                         D3D12_DESCRIPTOR_HEAP_FLAGS flags) :
        m_DeviceD3D12Impl{ device },
        m_HeapDesc
        {
            type,
            numDescriptorsInHeap + numDynamicDescriptors,
            flags,
            1
        },
        m_pd3d12DescriptorHeap(CreateHeap(device.GetD3D12Device(), m_HeapDesc)),
        m_DescriptorSize{ device.GetD3D12Device()->GetDescriptorHandleIncrementSize(type) },
        m_HeapAllocationManager{ device, *this, 0, m_pd3d12DescriptorHeap.Get(), 0, numDescriptorsInHeap },
        m_DynamicAllocationsManager{ device, *this, 1, m_pd3d12DescriptorHeap.Get(), numDescriptorsInHeap, numDynamicDescriptors }
    {
    }

    DescriptorHeapAllocation GPUDescriptorHeap::Allocate(QueueMask queueMask, uint32 count)
    {
        std::lock_guard<std::mutex> LockGuard(m_AllocMutex);

        DescriptorHeapAllocation allocation = m_HeapAllocationManager.Allocate(queueMask, count);
        return allocation;
    }

    DescriptorHeapAllocation GPUDescriptorHeap::AllocateDynamic(QueueMask queueMask, uint32 count)
    {
        std::lock_guard<std::mutex> LockGuard(m_DynAllocMutex);

        DescriptorHeapAllocation allocation = m_DynamicAllocationsManager.Allocate(queueMask, count);
        return allocation;
    }

    void GPUDescriptorHeap::SafeFree(DescriptorHeapAllocation&& allocation)
    {
        QueueMask queueMask = allocation.GetQueueMask();

        StaleAllocation staleAllocation(
            std::move(allocation),
            *this
        );

        ReleaseResourceWrapper releaseObj(queueMask);
        releaseObj.Set(std::move(staleAllocation));

        m_DeviceD3D12Impl.SafeReleaseObject(std::move(releaseObj));
    }

    void GPUDescriptorHeap::FreeAllocation(DescriptorHeapAllocation&& allocation)
    {
        auto mgrId = allocation.GetAllocationManagerId();

        if (mgrId == 0)
        {
            std::lock_guard<std::mutex> LockGuard(m_AllocMutex);
            m_HeapAllocationManager.FreeAllocation(std::move(allocation));
        }
        else
        {
            std::lock_guard<std::mutex> LockGuard(m_DynAllocMutex);
            m_DynamicAllocationsManager.FreeAllocation(std::move(allocation));
        }
    }

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GPUDescriptorHeap::CreateHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_DESC desc)
    {
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pd3d12DescriptorHeap;
        HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(pd3d12DescriptorHeap.GetAddressOf()));

        if (FAILED(hr))
            throw;

        return pd3d12DescriptorHeap;
    }
}