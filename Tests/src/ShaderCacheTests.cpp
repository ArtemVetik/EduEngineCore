#include "TestAsserts.h"

#include <ShaderCache.h>

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

using namespace EduEngine::EduBinding;
using EduEngine::Tests::AssertCapture;

namespace
{
	constexpr const wchar_t* TestShader = L"assets/Shaders/CacheTestCS.hlsl";
	constexpr const wchar_t* BrokenShader = L"assets/Shaders/BrokenCS.hlsl";
	constexpr const wchar_t* Target = L"cs_6_6";
}

/// Uses its own ShaderCache instance, not the process-wide one, so tests do not affect each other.
/// Shaders are really compiled with DXC, but no GPU is needed.
class ShaderCacheTests : public testing::Test
{
protected:
	std::shared_ptr<ShaderD3D12> Get(const wchar_t*	   path,
									 const wchar_t*	   entryPoint = L"CSMain",
									 const LPCWSTR*	   defines = nullptr,
									 const ShaderDesc& desc = ShaderDesc())
	{
		return m_Cache.GetOrCreate(path, entryPoint, Target, defines, desc);
	}

	ShaderCache m_Cache;
};

TEST_F(ShaderCacheTests, ReturnsSameShaderForSameInputs)
{
	auto first = Get(TestShader);
	auto second = Get(TestShader);

	ASSERT_NE(first, nullptr);
	EXPECT_TRUE(first->IsValid());
	EXPECT_EQ(first, second);

	EXPECT_EQ(m_Cache.GetShadersNum(), 1u);
	EXPECT_EQ(m_Cache.GetMissesNum(), 1u);
	EXPECT_EQ(m_Cache.GetHitsNum(), 1u);
}

TEST_F(ShaderCacheTests, TreatsDifferentPathSpellingsAsSameFile)
{
	auto shader = Get(L"assets/Shaders/CacheTestCS.hlsl");

	ASSERT_NE(shader, nullptr);
	EXPECT_EQ(Get(L"assets\\Shaders\\CacheTestCS.hlsl"), shader);
	EXPECT_EQ(Get(L"assets/shaders/./cachetestcs.hlsl"), shader);
	EXPECT_EQ(Get(L"ASSETS/Shaders/../Shaders/CacheTestCS.HLSL"), shader);

	EXPECT_EQ(m_Cache.GetMissesNum(), 1u);
}

TEST_F(ShaderCacheTests, IgnoresDefinesOrder)
{
	LPCWSTR defines[] = { L"GROUP_SIZE", L"32", L"SCALE", L"2", NULL };
	LPCWSTR reordered[] = { L"SCALE", L"2", L"GROUP_SIZE", L"32", NULL };

	auto shader = Get(TestShader, L"CSMain", defines);

	ASSERT_NE(shader, nullptr);
	EXPECT_EQ(Get(TestShader, L"CSMain", reordered), shader);
}

TEST_F(ShaderCacheTests, DifferentDefinesProduceDifferentShaders)
{
	LPCWSTR scale2[] = { L"SCALE", L"2", NULL };
	LPCWSTR scale3[] = { L"SCALE", L"3", NULL };

	auto noDefines = Get(TestShader);
	auto withScale2 = Get(TestShader, L"CSMain", scale2);
	auto withScale3 = Get(TestShader, L"CSMain", scale3);

	ASSERT_NE(noDefines, nullptr);
	ASSERT_NE(withScale2, nullptr);
	ASSERT_NE(withScale3, nullptr);

	EXPECT_NE(noDefines, withScale2);
	EXPECT_NE(withScale2, withScale3);
	EXPECT_EQ(m_Cache.GetShadersNum(), 3u);
}

TEST_F(ShaderCacheTests, DifferentEntryPointsProduceDifferentShaders)
{
	auto main = Get(TestShader, L"CSMain");
	auto clear = Get(TestShader, L"CSClear");

	ASSERT_NE(main, nullptr);
	ASSERT_NE(clear, nullptr);
	EXPECT_NE(main, clear);
}

TEST_F(ShaderCacheTests, IgnoresResourceDescOrder)
{
	ShaderResourceDesc resources[] =
	{
		{ "gOutput", SHADER_RESOURCE_TYPE_DYNAMIC },
		{ "cbParams", SHADER_RESOURCE_TYPE_DYNAMIC },
	};
	ShaderResourceDesc reordered[] =
	{
		{ "cbParams", SHADER_RESOURCE_TYPE_DYNAMIC },
		{ "gOutput", SHADER_RESOURCE_TYPE_DYNAMIC },
	};

	ShaderDesc desc;
	desc.ResourceNum = _countof(resources);
	desc.ResourceDesc = resources;

	ShaderDesc reorderedDesc;
	reorderedDesc.ResourceNum = _countof(reordered);
	reorderedDesc.ResourceDesc = reordered;

	auto shader = Get(TestShader, L"CSMain", nullptr, desc);

	ASSERT_NE(shader, nullptr);
	EXPECT_EQ(Get(TestShader, L"CSMain", nullptr, reorderedDesc), shader);
}

TEST_F(ShaderCacheTests, DifferentResourceTypesProduceDifferentShaders)
{
	// The resource layout is baked into ShaderResources, so it must be a part of the key
	ShaderResourceDesc dynamicOutput[] = { { "gOutput", SHADER_RESOURCE_TYPE_DYNAMIC } };
	ShaderResourceDesc mutableOutput[] = { { "gOutput", SHADER_RESOURCE_TYPE_MUTABLE } };

	ShaderDesc dynamicDesc;
	dynamicDesc.ResourceNum = 1;
	dynamicDesc.ResourceDesc = dynamicOutput;

	ShaderDesc mutableDesc;
	mutableDesc.ResourceNum = 1;
	mutableDesc.ResourceDesc = mutableOutput;

	ShaderDesc dynamicByDefaultDesc;
	dynamicByDefaultDesc.DefaultType = SHADER_RESOURCE_TYPE_DYNAMIC;

	auto dynamicShader = Get(TestShader, L"CSMain", nullptr, dynamicDesc);
	auto mutableShader = Get(TestShader, L"CSMain", nullptr, mutableDesc);
	auto dynamicByDefaultShader = Get(TestShader, L"CSMain", nullptr, dynamicByDefaultDesc);

	ASSERT_NE(dynamicShader, nullptr);
	ASSERT_NE(mutableShader, nullptr);
	ASSERT_NE(dynamicByDefaultShader, nullptr);

	EXPECT_NE(dynamicShader, mutableShader);
	EXPECT_NE(dynamicByDefaultShader, dynamicShader);
	EXPECT_NE(dynamicByDefaultShader, mutableShader);
}

TEST_F(ShaderCacheTests, FailedCompilationIsNotCached)
{
	AssertCapture asserts;

	EXPECT_EQ(Get(BrokenShader), nullptr);
	EXPECT_EQ(m_Cache.GetShadersNum(), 0u);

	// The failure is not remembered: the next request compiles the shader again
	EXPECT_EQ(Get(BrokenShader), nullptr);
	EXPECT_EQ(m_Cache.GetMissesNum(), 2u);

#if defined(DEBUG) | defined(_DEBUG)
	EXPECT_FALSE(asserts.GetMessages().empty());
#endif
}

TEST_F(ShaderCacheTests, MissingFileIsNotCached)
{
	AssertCapture asserts;

	EXPECT_EQ(Get(L"assets/Shaders/DoesNotExist.hlsl"), nullptr);
	EXPECT_EQ(m_Cache.GetShadersNum(), 0u);

#if defined(DEBUG) | defined(_DEBUG)
	EXPECT_FALSE(asserts.GetMessages().empty());
#endif
}

TEST_F(ShaderCacheTests, TrimReleasesOnlyUnusedShaders)
{
	auto kept = Get(TestShader, L"CSMain");
	Get(TestShader, L"CSClear");	// the returned reference is dropped right away

	ASSERT_EQ(m_Cache.GetShadersNum(), 2u);

	m_Cache.Trim();

	EXPECT_EQ(m_Cache.GetShadersNum(), 1u);
	EXPECT_EQ(Get(TestShader, L"CSMain"), kept);
}

TEST_F(ShaderCacheTests, ConcurrentRequestsCompileShaderOnce)
{
	constexpr int ThreadsNum = 8;

	std::vector<std::shared_ptr<ShaderD3D12>> results(ThreadsNum);
	std::vector<std::thread> threads;
	std::atomic<bool> start = false;

	for (int i = 0; i < ThreadsNum; i++)
	{
		threads.emplace_back([&, i]()
			{
				// start all threads at once to make them request the shader simultaneously
				while (!start)
					std::this_thread::yield();

				results[i] = Get(TestShader);
			});
	}

	start = true;

	for (auto& thread : threads)
		thread.join();

	ASSERT_NE(results[0], nullptr);

	for (int i = 1; i < ThreadsNum; i++)
		EXPECT_EQ(results[i], results[0]) << "Thread " << i << " got a different shader";

	// However the threads interleave, the shader is compiled exactly once,
	// every other request either waits for that compilation or hits the cache
	EXPECT_EQ(m_Cache.GetMissesNum(), 1u);
	EXPECT_EQ(m_Cache.GetHitsNum(), static_cast<uint32_t>(ThreadsNum - 1));
	EXPECT_EQ(m_Cache.GetShadersNum(), 1u);
}
