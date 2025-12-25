#include <crtdbg.h>
#include <sstream>
#include <chrono>
#include <iostream>
#include <vector>

#include "Allocators.h"

using Clock = std::chrono::steady_clock;

struct XorShift32
{
    uint32_t state = 0x12345678;

    uint32_t next()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }

    uint32_t range(uint32_t max)
    {
        return next() % max;
    }
};

struct BenchmarkConfig
{
    int iterations = 5'000'000;
    int maxLiveAllocs = 10000;
    float allocChance = 0.6f;
    int minSize = 524;
    int maxSize = 8096;
};


double benchmark_random(IAllocator& alloc, const BenchmarkConfig& cfg)
{
    std::vector<void*> live;
    live.reserve(cfg.maxLiveAllocs);

    XorShift32 rng;

    auto t0 = Clock::now();

    for (int i = 0; i < cfg.iterations; ++i)
    {
        bool doAlloc =
            live.empty() ||
            (live.size() < cfg.maxLiveAllocs &&
                (rng.next() / float(UINT32_MAX)) < cfg.allocChance);

        if (doAlloc)
        {
            int size = cfg.minSize + rng.range(cfg.maxSize - cfg.minSize + 1);
            void* p = alloc.alloc(size);
            live.push_back(p);
        }
        else
        {
            int idx = rng.range((uint32_t)live.size());
            alloc.free(live[idx]);
            live[idx] = live.back();
            live.pop_back();
        }
    }

    for (void* p : live)
        alloc.free(p);

    auto t1 = Clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void runTest(const char* name, const BenchmarkConfig& cfg, IAllocator& stdAllocator, IAllocator& customAllocator)
{
    printf("======== %s ========\n", name);
    double stdMs = benchmark_random(stdAllocator, cfg);
    double customMs = benchmark_random(customAllocator, cfg);

    printf("StdAllocator:    %lf ms\n", stdMs);
    printf("CustomAllocator: %lf ms\n", customMs);
    printf("============================\n\n");
}

int main()
{
    StdAllocator stdAllocator;
    CustomAllocator customAllocator;

    for (uint32 i = 0; i < 5'000'000; i++)
    {
        size_t size = (i * 8u) % (1024 * 1024);

        void* p0 = stdAllocator.alloc(size, 0);
        stdAllocator.free(p0);

        void* p1 = customAllocator.alloc(size, 0);
        customAllocator.free(p1);
    }

    {
        BenchmarkConfig cfg = {};
        cfg.iterations = 5'000'000;
        cfg.maxLiveAllocs = 10000;
        cfg.allocChance = 0.6f;
        cfg.minSize = 1;
        cfg.maxSize = 512;

        runTest("SmallAlloc", cfg, stdAllocator, customAllocator);
    }

    {
        BenchmarkConfig cfg = {};
        cfg.iterations = 1'000'000;
        cfg.maxLiveAllocs = 1000;
        cfg.allocChance = 0.6f;
        cfg.minSize = 514;
        cfg.maxSize = 1024 * 1024;

        runTest("MediuamAlloc", cfg, stdAllocator, customAllocator);
    }

    {
        BenchmarkConfig cfg = {};
        cfg.iterations = 100'000;
        cfg.maxLiveAllocs = 1000;
        cfg.allocChance = 0.6f;
        cfg.minSize = 1024 * 1024;
        cfg.maxSize = 64 * 1024 * 1024;

        runTest("BigAlloc", cfg, stdAllocator, customAllocator);
    }

    {
        BenchmarkConfig cfg = {};
        cfg.iterations = 1'000'000;
        cfg.maxLiveAllocs = 1000;
        cfg.allocChance = 0.6f;
        cfg.minSize = 4;
        cfg.maxSize = 32 * 1024 * 1024;

        runTest("MixedAlloc", cfg, stdAllocator, customAllocator);
    }

	return 0;
}