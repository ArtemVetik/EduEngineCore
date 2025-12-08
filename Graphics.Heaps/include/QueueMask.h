#pragma once
#include "framework.h"

namespace EduEngine
{
	enum GRAPHICS_HEAPS_API QueueMask
	{
		Direct = 1 << 0,
		Compute = 1 << 1,
		Copy = 1 << 2,
	};

	static constexpr uint8 MaxQueueMask = QueueMask::Direct | QueueMask::Compute | QueueMask::Copy;

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