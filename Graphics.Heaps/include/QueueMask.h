#pragma once
#include "framework.h"

#include <Asserts.h>

namespace EduEngine
{
	typedef uint8 QueueMask;

	enum GRAPHICS_HEAPS_API QueueId : QueueMask
	{
		Direct = 1 << 0,
		Compute = 1 << 1,
		Copy = 1 << 2,
	};

	static constexpr uint8 SupportedQueuesNum = 3;
	static constexpr QueueMask MaxQueueMask = QueueId::Direct | QueueId::Compute | QueueId::Copy;

	__forceinline D3D12_COMMAND_LIST_TYPE QueueIdToCmdListType(QueueId queueId)
	{
		switch (queueId)
		{
		case QueueId::Direct: return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case QueueId::Compute: return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case QueueId::Copy: return D3D12_COMMAND_LIST_TYPE_COPY;
		default: ASSERT_FAILED("QueueId \"", queueId, "\" is not supported!");
		}
	}

	__forceinline QueueId CmdListTypeToQueueId(D3D12_COMMAND_LIST_TYPE cmdListType)
	{
		switch (cmdListType)
		{
		case D3D12_COMMAND_LIST_TYPE_DIRECT: return QueueId::Direct;
		case D3D12_COMMAND_LIST_TYPE_COMPUTE: return QueueId::Compute;
		case D3D12_COMMAND_LIST_TYPE_COPY: return QueueId::Copy;
		default: ASSERT_FAILED("CommandListType \"", cmdListType, "\" is not supported!");
		}
	}

	struct GRAPHICS_HEAPS_API FenceValues
	{
		uint64_t DirectFence;
		uint64_t ComputeFence;

		bool operator<=(const FenceValues& other) const {
			return (DirectFence <= other.DirectFence) && (ComputeFence <= other.ComputeFence);
		}

		bool operator<(const FenceValues& other) const {
			return (DirectFence < other.DirectFence) && (ComputeFence <= other.ComputeFence);
		}
	};
}