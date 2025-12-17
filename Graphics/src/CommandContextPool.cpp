#include "CommandContextPool.h"

namespace EduEngine
{
	CommandContextPool::CommandContextPool(RenderDeviceD3D12& device) :
		m_Device(device)
	{
		m_CmdListMgrs = static_cast<CommandListManager*>(malloc(sizeof(CommandListManager) * SupportedQueuesNum));

		for (uint8 i = 0; i < SupportedQueuesNum; i++)
			new (&m_CmdListMgrs[i]) CommandListManager(device, QueueIdToCmdListType(static_cast<QueueId>(1 << i)));
	}

	CommandContextPool::~CommandContextPool()
	{
		for (uint8 i = 0; i < SupportedQueuesNum; i++)
			m_CmdListMgrs[i].~CommandListManager();

		free(m_CmdListMgrs);
	}

	std::unique_ptr<CommandContext> CommandContextPool::AllocateContext(QueueId queueId)
	{
		std::lock_guard<std::mutex> Lock(m_PoolMutex);

		auto poolIt = m_Pool.find(queueId);

		if (poolIt != m_Pool.end())
		{
			std::unique_ptr<CommandContext> ctx = std::move(poolIt->second);
			m_Pool.erase(poolIt);
			
			ctx->Reset();
			return std::move(ctx);
		}

		auto& cmdListMgr = m_CmdListMgrs[GetQueueIdIndex(queueId)];
		return std::make_unique<CommandContext>(m_Device, cmdListMgr);
	}

	void CommandContextPool::FreeContext(std::unique_ptr<CommandContext>&& context)
	{
		std::lock_guard<std::mutex> Lock(m_PoolMutex);

		QueueId queueId = CmdListTypeToQueueId(context->GetType());
		m_Pool.emplace(queueId, std::move(context));
	}
}