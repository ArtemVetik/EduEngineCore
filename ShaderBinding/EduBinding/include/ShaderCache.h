#pragma once
#include "framework.h"
#include "ShaderD3D12.h"

#include <atomic>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace EduEngine::EduBinding
{
	/// Identifies a unique shader. Contains everything ShaderD3D12 is built from:
	/// the compilation inputs (source file, entry point, target, defines) and the
	/// resource layout (ShaderDesc), which is baked into ShaderResources.
	struct ShaderCacheKey
	{
		std::wstring Path;			// normalized: lexically normal, lower case
		std::wstring EntryPoint;
		std::wstring Target;

		std::vector<std::wstring> Defines;									// "NAME=VALUE", sorted
		std::vector<std::pair<std::string, SHADER_RESOURCE_TYPE>> ResTypes;	// sorted by name
		SHADER_RESOURCE_TYPE DefaultResType = SHADER_RESOURCE_TYPE_MUTABLE;

		bool operator == (const ShaderCacheKey& rhs) const;
	};

	struct ShaderCacheKeyHasher
	{
		size_t operator()(const ShaderCacheKey& key) const;
	};

	/// Compiles every shader once and shares it between all pipeline states that ask
	/// for the same one. Compilation flags are not part of the key: a single build
	/// config is used by the whole process.
	class EDUBINDING_API ShaderCache
	{
	public:
		/// Process-wide cache. All modules are linked statically into one executable,
		/// so there is a single instance.
		static ShaderCache& Get();

		ShaderCache() = default;

		ShaderCache(const ShaderCache&) = delete;
		ShaderCache& operator = (const ShaderCache&) = delete;

		/// Returns an already compiled shader or compiles a new one.
		/// Returns nullptr if the compilation fails; failed shaders are not cached.
		std::shared_ptr<ShaderD3D12> GetOrCreate(const wchar_t*	   name,
												 const wchar_t*	   entryPoint,
												 const wchar_t*	   target,
												 const LPCWSTR*	   defines,
												 const ShaderDesc& desc);

		/// Releases the shaders that nobody except the cache references
		void Trim();
		void Clear();

		uint32 GetShadersNum() const;
		uint32 GetHitsNum() const { return m_HitsNum; }
		uint32 GetMissesNum() const { return m_MissesNum; }

	private:
		struct CacheEntry
		{
			std::shared_ptr<ShaderD3D12>					 Shader;	// set when the compilation is done
			std::shared_future<std::shared_ptr<ShaderD3D12>> Pending;	// valid while another thread compiles the shader
		};

		mutable std::mutex m_Mutex;
		std::unordered_map<ShaderCacheKey, CacheEntry, ShaderCacheKeyHasher> m_Entries;

		std::atomic<uint32> m_HitsNum = 0;
		std::atomic<uint32> m_MissesNum = 0;
	};
}
