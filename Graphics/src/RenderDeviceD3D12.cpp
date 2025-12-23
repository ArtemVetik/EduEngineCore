#include "pch.h"
#include "RenderDeviceD3D12.h"
#include "CommandQueueD3D12.h"
#include "CommandContext.h"

namespace EduEngine
{
	RenderDeviceD3D12::RenderDeviceD3D12(Microsoft::WRL::ComPtr<ID3D12Device> device, QueueMask commandQueues, const QueryHeapSettings& queryDesc) :
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
		m_GlobalDynamicHeap{ this, 1, 1 << 20 },
		m_ActiveQueues(commandQueues),
		m_QueueCount(__popcnt(commandQueues)),
		m_CmdContextPool(*this)
	{
		VERIFY_EXPR(m_ActiveQueues > 0 && m_ActiveQueues <= MaxQueueMask, "Invalid \"m_ActiveQueues\" value: ", m_ActiveQueues);
		VERIFY_EXPR(m_QueueCount > 0 && m_QueueCount <= SupportedQueuesNum, "Invalid \"m_QueueCount\" value: ", m_QueueCount);

		m_NextAviableContextId = 0;

		if (queryDesc.NumQueries > 0)
			m_QueryHeap = new QueryHeap(this, queryDesc.NumQueries, queryDesc.Type);
		else
			m_QueryHeap = nullptr;

		uint8 qIndex = 0;
		m_CommandQueues = (CommandQueueD3D12*)malloc(sizeof(CommandQueueD3D12) * m_QueueCount);

		for (uint8 i = 0; i < SupportedQueuesNum; i++)
		{
			QueueId queueId = (QueueId)(1 << i);

			if (commandQueues & queueId)
				new (&m_CommandQueues[qIndex++]) CommandQueueD3D12(this, QueueIdToCmdListType(queueId));
		}
	}

	RenderDeviceD3D12::~RenderDeviceD3D12()
	{
		FinishFrame(true);
		m_GlobalDynamicHeap.Destroy();
		
		if (m_QueryHeap)
		{
			delete m_QueryHeap;
			m_QueryHeap = nullptr;
		}

		for (uint8 i = 0; i < m_QueueCount; i++)
			m_CommandQueues[i].~CommandQueueD3D12();

		free(m_CommandQueues);
	}

	DescriptorHeapAllocation RenderDeviceD3D12::AllocateCPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count)
	{
		assert(type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
		return m_CPUDescriptorHeaps[type].Allocate(count);
	}

	DescriptorHeapAllocation RenderDeviceD3D12::AllocateGPUDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t count)
	{
		assert(type >= D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type <= D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		return m_GPUDescriptorHeaps[type].Allocate(count);
	}
	
	CommandQueueD3D12& RenderDeviceD3D12::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type)
	{
		return GetCommandQueue(CmdListTypeToQueueId(type));
	}

	const QueryHeap& RenderDeviceD3D12::GetQueryHeap() const
	{
		VERIFY_EXPR(m_QueryHeap != nullptr, "QueryHeap was not created!");
		return *m_QueryHeap;
	}

	void RenderDeviceD3D12::SafeReleaseObject(ReleaseResourceWrapper&& wrapper, QueueMask queueMask)
	{
		queueMask &= m_ActiveQueues;
		VERIFY_EXPR(queueMask > 0 && queueMask <= MaxQueueMask, "");

		uint16 numReferences = __popcnt16(queueMask);

		ReleaseResource releaseRes(std::move(wrapper), numReferences);

		for (uint8 i = 0; i < m_QueueCount; i++)
		{
			QueueId queueId = (QueueId)((1 << i));

			if (queueMask & queueId)
				GetCommandQueue(queueId).SafeReleaseObject(releaseRes);
		}

		releaseRes.ReleaseOwnership();
	}

	void RenderDeviceD3D12::FinishFrame(bool forceRelease /* = false */)
	{
		// Release stale resources from all queues
		// TODO: release only if needed
		for (int i = 0; i < m_QueueCount; i++)
			m_CommandQueues[i].CloseAndExecuteCommandContexts(nullptr, 0);

		for (int i = 0; i < m_QueueCount; i++)
			m_CommandQueues[i].ProcessReleaseQueue(forceRelease);
	}

	void RenderDeviceD3D12::FlushQueues()
	{
		for (int i = 0; i < m_QueueCount; i++)
			m_CommandQueues[i].Flush();
	}

	ID3D12DescriptorHeap* RenderDeviceD3D12::GetD3D12DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) const
	{
		VERIFY_EXPR(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Incorrect heap type");

		return type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ?
			m_GPUDescriptorHeaps[0].GetD3D12Heap() :
			m_GPUDescriptorHeaps[1].GetD3D12Heap();
	}

	CommandQueueD3D12& RenderDeviceD3D12::GetCommandQueue(QueueId queueId)
	{
		VERIFY_EXPR(HasCommandQueue(queueId), "Requested command queue type was not created");

		uint8 queueIndex = __popcnt(m_ActiveQueues & ((queueId << 1) - 1)) - 1;
		return m_CommandQueues[queueIndex];
	}

	GPUDescriptorHeap& RenderDeviceD3D12::GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type)
	{
		VERIFY_EXPR(type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, "Incorrect heap type");

		return type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ?
			m_GPUDescriptorHeaps[0] :
			m_GPUDescriptorHeaps[1];
	}

	uint32 RenderDeviceD3D12::AllocateContextId()
	{
		if (m_AvailableContextIds.empty())
		{
			m_NextAviableContextId++;
			return m_NextAviableContextId - 1;
		}

		uint32 id = m_AvailableContextIds.back();
		m_AvailableContextIds.pop_back();

		return id;
	}

	void RenderDeviceD3D12::FreeContextId(uint32 id)
	{
		VERIFY_EXPR(std::find(m_AvailableContextIds.begin(), m_AvailableContextIds.end(), id) == m_AvailableContextIds.end(),
			"Failed to free context id (", id, "): m_AvailableContextIds already has this id");

		m_AvailableContextIds.push_back(id);
	}
}