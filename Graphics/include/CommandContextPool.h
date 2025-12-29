#pragma once
#include "framework.h"
#include "CommandContext.h"

#include <RawMemoryAllocator.h>
#include <QueueMask.h>
#include <unordered_map>

namespace EduEngine
{
	class GRAPHICS_API CommandContextPool
	{
	public:
		CommandContextPool(RenderDeviceD3D12& device);
		~CommandContextPool();

		std::unique_ptr<CommandContext> AllocateContext(QueueId queueId);
		void FreeContext(std::unique_ptr<CommandContext>&& context);

	private:
		std::mutex m_PoolMutex;

		CommandListManager* m_CmdListMgrs;

		using PoolAlloc = MemoryAllocator::RawMemoryAllocator<std::pair<const QueueId, std::unique_ptr<CommandContext>>>;
		std::unordered_multimap<QueueId, std::unique_ptr<CommandContext>, std::hash<QueueId>, std::equal_to<QueueId>, PoolAlloc> m_Pool;

		RenderDeviceD3D12& m_Device;
	};
}