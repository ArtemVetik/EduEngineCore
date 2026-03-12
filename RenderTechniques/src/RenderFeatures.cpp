#include "RenderFeatures.h"

namespace EduEngine
{
	RenderFeatures g_RenderFeatures;
	PSOCache g_PsoCache;

	PSOEntry::PSOEntry()
	{
		g_PsoCache.Register(this); // TODO: make RAII wrapper
	}

	PSOEntry::~PSOEntry()
	{
		g_PsoCache.Unregister(this);
	}

	void PSOEntry::Initialize()
	{
		CurrentKey = MakeKeyFromFeatures(g_RenderFeatures);
		Pso = BuildPsoFunc();
		OnPsoUpdated();
	}

	std::string PSOEntry::MakeKeyFromFeatures(const RenderFeatures& f) const
	{
		std::ostringstream ss;

		auto AddDefine = [&](const char* name, const auto& val)
			{
				ss << name << '=' << val << ';';
			};

		AddDefine("NAME", Name);

		for (auto& param : DependentParams)
		{
			if (param == RenderFeatureID::UseSSAO) AddDefine("USE_SSAO", f.UseSSAO);
			else if (param == RenderFeatureID::UseIBL) AddDefine("USE_IBL", f.UseIBL);
			else if (param == RenderFeatureID::PackNormalsMethod) AddDefine("PACK_NORMALS", (int)f.PackNormalsMethod);
			else if (param == RenderFeatureID::SSRTraceMethod) AddDefine("SSR_TRACE_METHOD", (int)f.SSRTraceMethod);
			else if (param == RenderFeatureID::DebugView) AddDefine("DEBUG_VIEW", (int)f.DebugView);
		}

		return ss.str();
	}

	void PSOCache::Register(PSOEntry* entry)
	{
		m_AllEntries.push_back(entry);
	}

	void PSOCache::Unregister(PSOEntry* entry)
	{
		for (auto iter = m_AllEntries.begin(); iter != m_AllEntries.end(); ++iter)
		{
			if (*iter == entry)
			{
				m_AllEntries.erase(iter);
				break;
			}
		}
	}

	void PSOCache::OnRenderFeaturesChanged(const RenderFeatures& f, RenderFeatureID changed)
	{
		for (auto* entry : m_AllEntries) {
			if (!EntryDependsOn(entry, changed))
				continue;

			UpdateEntry(*entry, f);
		}
	}

	void PSOCache::Clear()
	{
		m_Cache.clear();
		m_AllEntries.clear();
	}

	void PSOCache::UpdateEntry(PSOEntry& entry, const RenderFeatures& f)
	{
		std::string newKey = entry.MakeKeyFromFeatures(f);
		if (newKey == entry.CurrentKey)
			return;

		auto cachdIt = m_Cache.find(newKey);
		if (cachdIt != m_Cache.end())
		{
			entry.Pso = cachdIt->second;
			entry.OnPsoUpdated();
			entry.CurrentKey = newKey;
			return;
		}

		std::shared_ptr<PipelineStateBase> newPso = entry.BuildPsoFunc();

		m_Cache.insert(std::make_pair(newKey, newPso)); // TODO: if cache too large, delete old entries
		entry.Pso = newPso;
		entry.OnPsoUpdated();
		entry.CurrentKey = newKey;
	}

	bool PSOCache::EntryDependsOn(PSOEntry* entry, RenderFeatureID changed)
	{
		return std::find(entry->DependentParams.begin(), entry->DependentParams.end(), changed) != entry->DependentParams.end();
	}
}