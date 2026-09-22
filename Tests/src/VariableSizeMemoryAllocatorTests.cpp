#include <VariableSizeMemoryAllocator.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <random>
#include <vector>

using namespace EduEngine;

namespace
{
	constexpr size_t InvalidOffset = VariableSizeMemoryAllocator::InvalidOffset;

	struct Allocation
	{
		size_t Offset;
		size_t Size;
	};

	/// Length of the longest run of free units in the reference model
	size_t LongestFreeRun(const std::vector<bool>& used)
	{
		size_t longest = 0;
		size_t current = 0;

		for (bool isUsed : used)
		{
			current = isUsed ? 0 : current + 1;
			longest = std::max(longest, current);
		}

		return longest;
	}
}

TEST(VariableSizeMemoryAllocatorTests, NewAllocatorIsEntirelyFree)
{
	VariableSizeMemoryAllocator allocator(256);

	EXPECT_EQ(allocator.GetFreeSize(), 256);
}

TEST(VariableSizeMemoryAllocatorTests, AllocatesSequentially)
{
	VariableSizeMemoryAllocator allocator(256);

	EXPECT_EQ(allocator.Allocate(16), 0);
	EXPECT_EQ(allocator.Allocate(32), 16);
	EXPECT_EQ(allocator.Allocate(8), 48);
	EXPECT_EQ(allocator.GetFreeSize(), 256 - 16 - 32 - 8);
}

TEST(VariableSizeMemoryAllocatorTests, FailsWhenRequestIsLargerThanFreeSpace)
{
	VariableSizeMemoryAllocator allocator(256);

	EXPECT_EQ(allocator.Allocate(257), InvalidOffset);
	EXPECT_EQ(allocator.GetFreeSize(), 256);
}

TEST(VariableSizeMemoryAllocatorTests, ExactFitFillsTheWholeSpace)
{
	VariableSizeMemoryAllocator allocator(256);

	EXPECT_EQ(allocator.Allocate(256), 0);
	EXPECT_EQ(allocator.GetFreeSize(), 0);
	EXPECT_EQ(allocator.Allocate(1), InvalidOffset);
}

TEST(VariableSizeMemoryAllocatorTests, ReusesFreedBlock)
{
	VariableSizeMemoryAllocator allocator(64);

	size_t first = allocator.Allocate(32);
	allocator.Allocate(32);

	allocator.Free(first, 32);

	EXPECT_EQ(allocator.Allocate(32), first);
}

TEST(VariableSizeMemoryAllocatorTests, CoalescesNeighboursInAnyFreeOrder)
{
	// Three adjacent blocks freed in every possible order must merge back
	// into a single block that covers the whole space
	std::array<int, 3> order = { 0, 1, 2 };

	do
	{
		SCOPED_TRACE(testing::Message() << "Free order: " << order[0] << ", " << order[1] << ", " << order[2]);

		VariableSizeMemoryAllocator allocator(96);

		std::array<size_t, 3> offsets;
		for (size_t& offset : offsets)
			offset = allocator.Allocate(32);

		for (int idx : order)
			allocator.Free(offsets[idx], 32);

		EXPECT_EQ(allocator.GetFreeSize(), 96);
		EXPECT_EQ(allocator.Allocate(96), 0);
	}
	while (std::next_permutation(order.begin(), order.end()));
}

TEST(VariableSizeMemoryAllocatorTests, FragmentedSpaceCannotServeLargeRequest)
{
	VariableSizeMemoryAllocator allocator(100);

	std::vector<size_t> offsets;
	for (int i = 0; i < 10; i++)
		offsets.push_back(allocator.Allocate(10));

	// Free every other block: 50 units are free, but no two of them are adjacent
	for (int i = 0; i < 10; i += 2)
		allocator.Free(offsets[i], 10);

	EXPECT_EQ(allocator.GetFreeSize(), 50);
	EXPECT_EQ(allocator.Allocate(20), InvalidOffset);
	EXPECT_NE(allocator.Allocate(10), InvalidOffset);
}

TEST(VariableSizeMemoryAllocatorTests, PicksSmallestFittingBlock)
{
	VariableSizeMemoryAllocator allocator(100);

	size_t a = allocator.Allocate(30);	// [0, 30)
	allocator.Allocate(10);				// [30, 40)
	size_t c = allocator.Allocate(10);	// [40, 50)
	allocator.Allocate(50);				// [50, 100)

	allocator.Free(a, 30);
	allocator.Free(c, 10);

	// Both free blocks can serve the request, the 10-unit one fits best
	EXPECT_EQ(allocator.Allocate(10), c);
	EXPECT_EQ(allocator.Allocate(30), a);
}

TEST(VariableSizeMemoryAllocatorTests, RandomizedAgainstReferenceModel)
{
	// Runs a long random sequence of Allocate/Free and after every step checks the allocator
	// against a trivial model that marks every unit of the space as used or free
	constexpr size_t MaxSize = 1024;
	constexpr size_t MaxAllocationSize = 64;
	constexpr int StepsNum = 20000;

	VariableSizeMemoryAllocator allocator(MaxSize);

	std::vector<bool> used(MaxSize, false);
	std::vector<Allocation> live;

	std::mt19937 rng(12345);	// fixed seed keeps failures reproducible
	std::uniform_int_distribution<size_t> sizeDist(1, MaxAllocationSize);
	std::uniform_int_distribution<int> actionDist(0, 99);

	for (int step = 0; step < StepsNum; step++)
	{
		SCOPED_TRACE(testing::Message() << "Step " << step);

		bool allocate = live.empty() || actionDist(rng) < 55;

		if (allocate)
		{
			size_t size = sizeDist(rng);
			size_t offset = allocator.Allocate(size);

			if (offset == InvalidOffset)
			{
				// The free blocks are fully coalesced, so a request fails only when
				// there really is no contiguous free range large enough
				ASSERT_LT(LongestFreeRun(used), size);
			}
			else
			{
				ASSERT_LE(offset + size, MaxSize);

				bool overlaps = std::any_of(used.begin() + offset, used.begin() + offset + size, [](bool u) { return u; });
				ASSERT_FALSE(overlaps) << "Allocation [" << offset << ", " << offset + size << ") overlaps a live one";

				std::fill(used.begin() + offset, used.begin() + offset + size, true);
				live.push_back({ offset, size });
			}
		}
		else
		{
			size_t idx = std::uniform_int_distribution<size_t>(0, live.size() - 1)(rng);
			Allocation allocation = live[idx];

			live[idx] = live.back();
			live.pop_back();

			allocator.Free(allocation.Offset, allocation.Size);
			std::fill(used.begin() + allocation.Offset, used.begin() + allocation.Offset + allocation.Size, false);
		}

		ASSERT_EQ(allocator.GetFreeSize(), static_cast<size_t>(std::count(used.begin(), used.end(), false)));
	}

	for (const Allocation& allocation : live)
		allocator.Free(allocation.Offset, allocation.Size);

	EXPECT_EQ(allocator.GetFreeSize(), MaxSize);
	EXPECT_EQ(allocator.Allocate(MaxSize), 0);
}
