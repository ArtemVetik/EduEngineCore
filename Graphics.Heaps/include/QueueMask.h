#pragma once
#include "framework.h"

namespace EduEngine
{
	typedef uint8 QueueMask;

	enum GRAPHICS_HEAPS_API QueueId : QueueMask
	{
		Direct = 1 << 0,
		Compute = 1 << 1,
		Copy = 1 << 2,
	};

	static constexpr QueueMask MaxQueueMask = QueueId::Direct | QueueId::Compute | QueueId::Copy;

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