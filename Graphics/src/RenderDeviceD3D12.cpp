#include "pch.h"
#include "RenderDeviceD3D12.h"
#include "CommandQueueD3D12.h"
#include "CommandContext.h"

namespace EduEngine
{
	RenderDeviceD3D12::RenderDeviceD3D12(Microsoft::WRL::ComPtr<ID3D12Device> device) :
		mDevice(device),
		m_CPUDescriptorHeaps
		{
			{ *this, 64, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
			{ *this, 32, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
			{ *this, 16, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE},
			{ *this, 16, D3D12_DESCRIPTOR_HEAP_TYPE_DSV,         D3D12_DESCRIPTOR_HEAP_FLAG_NONE}
		},
		m_GPUDescriptorHeaps
		{
			{ *this, 65536, 32768,		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE},
			{ *this, 1024,  1024 - 128, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,     D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE}
		},
		m_CommandQueues
		{
			{ this, D3D12_COMMAND_LIST_TYPE_DIRECT  },
			{ this, D3D12_COMMAND_LIST_TYPE_COMPUTE }
		},
		m_GlobalDynamicHeap{ this, 1, 1 << 20 },
		m_QueryHeap{ this, 16, D3D12_QUERY_HEAP_TYPE_TIMESTAMP }
	{
		m_AvailableContextIds.resize(MaxDeviceContexts);

		for (uint32 i = 0; i < MaxDeviceContexts; i++)
			m_AvailableContextIds[MaxDeviceContexts - 1 - i ] = i;
	}

	RenderDeviceD3D12::~RenderDeviceD3D12()
	{
		FinishFrame(true);
		m_GlobalDynamicHeap.Destroy();
	}

	DescriptorHeapAllocation RenderDeviceD3D12::AllocateCPUDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count)
	{
		assert(type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
		return m_CPUDescriptorHeaps[type].Allocate(queueMask, count);
	}

	DescriptorHeapAllocation RenderDeviceD3D12::AllocateGPUDescriptor(QueueMask queueMask, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count)
	{
		assert(type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		return m_GPUDescriptorHeaps[type].Allocate(queueMask, count);
	}

	CommandQueueD3D12& RenderDeviceD3D12::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type)
	{
		assert(type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE);
		if (type == D3D12_COMMAND_LIST_TYPE_DIRECT)
			return m_CommandQueues[0];
		if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
			return m_CommandQueues[1];
	}

	void RenderDeviceD3D12::SafeReleaseObject(QueueMask queueMask, ReleaseResourceWrapper&& wrapper)
	{
		VERIFY_EXPR(queueMask > 0 && queueMask <= MaxQueueMask, "");
		
		if (queueMask == QueueMask::Direct)
			m_CommandQueues[0].SafeReleaseObject(std::move(wrapper));
		else if (queueMask == QueueMask::Compute)
			m_CommandQueues[1].SafeReleaseObject(std::move(wrapper));
		//else if (queueMask == QueueMask::Both)
		//	this->SafeReleaseObject(std::move(wrapper));
	}

	void RenderDeviceD3D12::FinishFrame(bool forceRelease /* = false */)
	{
		for (int i = 0; i < 2; i++)
			m_CommandQueues[i].ProcessReleaseQueue(forceRelease);

		std::lock_guard<std::mutex> LockGuard(m_ReleasedObjectsMutex);

		auto numDirectCompletedCmdLists = m_CommandQueues[0].GetCompletedFenceNum();
		auto numComputeCompletedCmdLists = m_CommandQueues[1].GetCompletedFenceNum();
		auto numDirectNextCmdLists = m_CommandQueues[0].GetNextCmdListNum();
		auto numComputeNextCmdLists = m_CommandQueues[1].GetNextCmdListNum();

		FenceValues completedFences = { numDirectCompletedCmdLists, numComputeCompletedCmdLists };

		while (!m_ReleaseObjectsQueue.empty())
		{
			auto& firstObj = m_ReleaseObjectsQueue.front();
			// GPU must have been idled when ForceRelease == true 
			if (firstObj.first < completedFences || forceRelease)
				m_ReleaseObjectsQueue.pop_front();
			else
				break;
		}
	}

	void RenderDeviceD3D12::FlushQueues()
	{
		for (int i = 0; i < 2; i++)
			m_CommandQueues[i].Flush();
	}

	ID3D12DescriptorHeap* RenderDeviceD3D12::GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) const
	{
		VERIFY_EXPR(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Incorrect heap type");

		return type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ?
			m_GPUDescriptorHeaps[0].GetD3D12Heap() :
			m_GPUDescriptorHeaps[1].GetD3D12Heap();
	}

	void RenderDeviceD3D12::SafeReleaseObject(ReleaseResourceWrapper&& wrapper)
	{
		uint64_t directNum = m_CommandQueues[0].GetNextCmdListNum();
		uint64_t computeNum = m_CommandQueues[1].GetNextCmdListNum();

		m_ReleaseObjectsQueue.emplace_back(FenceValues{ directNum, computeNum }, std::move(wrapper));
	}

	GPUDescriptorHeap& RenderDeviceD3D12::GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		VERIFY_EXPR(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Incorrect heap type");

		return type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ?
			m_GPUDescriptorHeaps[0] :
			m_GPUDescriptorHeaps[1];
	}

	uint32 RenderDeviceD3D12::GetAvailableContextId()
	{
		if (m_AvailableContextIds.empty())
		{
			ASSERT_FAILED("There are no free context id's");
			return -1;
		}

		uint32 id = m_AvailableContextIds.back();
		m_AvailableContextIds.pop_back();

		return id;
	}
}