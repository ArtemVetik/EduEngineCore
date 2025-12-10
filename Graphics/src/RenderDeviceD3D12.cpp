#include "pch.h"
#include "RenderDeviceD3D12.h"
#include "CommandQueueD3D12.h"
#include "CommandContext.h"

namespace EduEngine
{
	RenderDeviceD3D12::RenderDeviceD3D12(Microsoft::WRL::ComPtr<ID3D12Device> device, uint8 commandQueuesCount) :
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
		m_QueueCount(commandQueuesCount)
	{
		m_AvailableContextIds.resize(MaxDeviceContexts);

		for (uint32 i = 0; i < MaxDeviceContexts; i++)
			m_AvailableContextIds[MaxDeviceContexts - 1 - i] = i;

		m_QueryHeap = new QueryHeap(this, 16, D3D12_QUERY_HEAP_TYPE_TIMESTAMP);

		VERIFY_EXPR(m_QueueCount > 0 && m_QueueCount <= 3, "");

		D3D12_COMMAND_LIST_TYPE types[]
		{
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			D3D12_COMMAND_LIST_TYPE_COMPUTE,
			D3D12_COMMAND_LIST_TYPE_COPY,
		};

		m_CommandQueues = (CommandQueueD3D12*)malloc(sizeof(CommandQueueD3D12) * m_QueueCount);

		for (uint8 i = 0; i < m_QueueCount; i++)
			new (&m_CommandQueues[i]) CommandQueueD3D12(this, types[i]);
	}

	RenderDeviceD3D12::~RenderDeviceD3D12()
	{
		FinishFrame(true);
		m_GlobalDynamicHeap.Destroy();

		delete m_QueryHeap;

		for (uint8 i = 0; i < m_QueueCount; i++)
			m_CommandQueues[i].~CommandQueueD3D12();

		free(m_CommandQueues);
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
		VERIFY_EXPR(type == D3D12_COMMAND_LIST_TYPE_DIRECT || type == D3D12_COMMAND_LIST_TYPE_COMPUTE || type == D3D12_COMMAND_LIST_TYPE_COPY, "");

		static constexpr uint8 IndexMap[] =
		{
			0, // D3D12_COMMAND_LIST_TYPE_DIRECT  -> index 0
			1, // D3D12_COMMAND_LIST_TYPE_BUNDLE  -> 
			1, // D3D12_COMMAND_LIST_TYPE_COMPUTE -> index 1
			2  // D3D12_COMMAND_LIST_TYPE_COPY    -> index 2
		};

		uint8 index = IndexMap[type];

		VERIFY_EXPR(index < m_QueueCount, "Requested command queue type was not created");

		return m_CommandQueues[index];
	}

	void RenderDeviceD3D12::SafeReleaseObject(ReleaseResourceWrapper&& wrapper)
	{
		ReleaseResourceWrapper copyWrapper = std::move(wrapper);

		uint32 queueMask = copyWrapper.GetQueueMask();

		VERIFY_EXPR(queueMask > 0 && queueMask <= MaxQueueMask, "");

		for (uint8 i = 0; i < m_QueueCount; i++)
		{
			if (queueMask & (1 << i))
				m_CommandQueues[i].SafeReleaseObject(copyWrapper);
		}

		copyWrapper.ReleaseOwnership();
	}

	void RenderDeviceD3D12::FinishFrame(bool forceRelease /* = false */)
	{
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