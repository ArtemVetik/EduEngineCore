#include "ShaderCache.h"

#include <Asserts.h>
#include <StringUtils.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace EduEngine::EduBinding
{
	// Source: https://www.boost.org/doc/libs/latest/libs/container_hash/doc/html/hash.html#notes_hash_combine
	__forceinline void HashCombine(size_t& seed, size_t value)
	{
		seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}

	/// Makes different spellings of the same file ("assets\\Shaders\\X.hlsl",
	/// "assets/shaders/./X.hlsl") produce the same key
	static std::wstring NormalizePath(const wchar_t* path)
	{
		std::wstring result = std::filesystem::path(path).lexically_normal().wstring();

		std::transform(result.begin(), result.end(), result.begin(),
					   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

		return result;
	}

	static ShaderCacheKey MakeKey(const wchar_t*	name,
								  const wchar_t*	entryPoint,
								  const wchar_t*	target,
								  const LPCWSTR*	defines,
								  const ShaderDesc& desc)
	{
		ShaderCacheKey key;
		key.Path = NormalizePath(name);
		key.EntryPoint = entryPoint;
		key.Target = target;
		key.DefaultResType = desc.DefaultType;

		if (defines)
		{
			for (uint32 i = 0; defines[i] != NULL; i += 2)
			{
				VERIFY_EXPR(defines[i + 1] != NULL, "Shader define \"", WCharToString(defines[i]), "\" has no value");

				key.Defines.push_back(std::wstring(defines[i]).append(L"=").append(defines[i + 1]));
			}

			// the order the defines are passed in does not change the compilation result
			std::sort(key.Defines.begin(), key.Defines.end());
		}

		key.ResTypes.reserve(desc.ResourceNum);
		for (uint16 i = 0; i < desc.ResourceNum; i++)
			key.ResTypes.emplace_back(desc.ResourceDesc[i].Name, desc.ResourceDesc[i].Type);

		std::sort(key.ResTypes.begin(), key.ResTypes.end());

		return key;
	}

	bool ShaderCacheKey::operator == (const ShaderCacheKey& rhs) const
	{
		return Path == rhs.Path &&
			EntryPoint == rhs.EntryPoint &&
			Target == rhs.Target &&
			DefaultResType == rhs.DefaultResType &&
			Defines == rhs.Defines &&
			ResTypes == rhs.ResTypes;
	}

	size_t ShaderCacheKeyHasher::operator()(const ShaderCacheKey& key) const
	{
		size_t seed = std::hash<std::wstring>{}(key.Path);

		HashCombine(seed, std::hash<std::wstring>{}(key.EntryPoint));
		HashCombine(seed, std::hash<std::wstring>{}(key.Target));
		HashCombine(seed, static_cast<size_t>(key.DefaultResType));

		for (const auto& define : key.Defines)
			HashCombine(seed, std::hash<std::wstring>{}(define));

		for (const auto& resType : key.ResTypes)
		{
			HashCombine(seed, std::hash<std::string>{}(resType.first));
			HashCombine(seed, static_cast<size_t>(resType.second));
		}

		return seed;
	}

	ShaderCache& ShaderCache::Get()
	{
		static ShaderCache instance;
		return instance;
	}

	std::shared_ptr<ShaderD3D12> ShaderCache::GetOrCreate(const wchar_t*	name,
														  const wchar_t*	entryPoint,
														  const wchar_t*	target,
														  const LPCWSTR*	defines,
														  const ShaderDesc& desc)
	{
		VERIFY_EXPR(name != nullptr, "Name must not be null");
		VERIFY_EXPR(entryPoint != nullptr, "EntryPoint must not be null");
		VERIFY_EXPR(target != nullptr, "Target must not be null");

		ShaderCacheKey key = MakeKey(name, entryPoint, target, defines, desc);

		std::shared_future<std::shared_ptr<ShaderD3D12>> pending;
		std::shared_ptr<std::promise<std::shared_ptr<ShaderD3D12>>> promise;

		{
			std::lock_guard<std::mutex> lock(m_Mutex);

			auto it = m_Entries.find(key);
			if (it != m_Entries.end())
			{
				if (it->second.Shader)
				{
					m_HitsNum++;
					return it->second.Shader;
				}

				// another thread is compiling this very shader right now
				pending = it->second.Pending;
			}
			else
			{
				promise = std::make_shared<std::promise<std::shared_ptr<ShaderD3D12>>>();

				CacheEntry entry;
				entry.Pending = promise->get_future().share();

				m_Entries.emplace(key, std::move(entry));
				m_MissesNum++;
			}
		}

		if (!promise)
		{
			m_HitsNum++;
			return pending.get();
		}

		auto shader = std::make_shared<ShaderD3D12>(name, entryPoint, target, defines, desc);

		if (!shader->IsValid())
			shader = nullptr;

		{
			std::lock_guard<std::mutex> lock(m_Mutex);

			auto it = m_Entries.find(key);
			if (it != m_Entries.end())
			{
				if (shader)
				{
					it->second.Shader = shader;
					it->second.Pending = {};
				}
				else
				{
					m_Entries.erase(it);
				}
			}
		}

		promise->set_value(shader);

		return shader;
	}

	void ShaderCache::Trim()
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		for (auto it = m_Entries.begin(); it != m_Entries.end();)
		{
			if (it->second.Shader && it->second.Shader.use_count() == 1)
				it = m_Entries.erase(it);
			else
				++it;
		}
	}

	void ShaderCache::Clear()
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		m_Entries.clear();
	}

	uint32 ShaderCache::GetShadersNum() const
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		return static_cast<uint32>(m_Entries.size());
	}
}
