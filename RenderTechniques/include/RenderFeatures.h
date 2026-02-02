#pragma once
#include <EngineTypes.h>
#include <sstream>
#include <functional>
#include <vector>
#include <unordered_map>
#include <PipelineState.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class RenderFeatures;
	class PSOCache;

	extern RenderFeatures g_RenderFeatures;
	extern PSOCache g_PsoCache;

	enum class RenderFeatureID
	{
		UseSSAO,
		UseIBL,
		PackNormalsMethod,
		DebugView,
	};

	enum class PackNormalsMethod : uint32
	{
		None, Simple, Spherical, Spheremap, Stereographic
	};

	enum class DebugView : uint32
	{
		None, Roughness, Metallic, AO, Normal, DiffuseIBL, SpecularIBL, NdotV, Fresnel, BRDFy, BRDFx, SSAO,
	};

	inline static const char* PackNormalsMethodStr[5] =
	{
		"None", "Simple", "Spherical", "Spheremap", "Stereographic"
	};

	inline static const char* DebugViewsStr[12] =
	{
		"NONE", "DEBUGVIEW_ROUGHNESS", "DEBUGVIEW_METALLIC", "DEBUGVIEW_AO",
		"DEBUGVIEW_NORMAL", "DEBUGVIEW_DIFFUSE_IBL", "DEBUGVIEW_SPECULAR_IBL",
		"DEBUGVIEW_NDOTV", "DEBUGVIEW_FRESNEL", "DEBUGVIEW_BRDF_Y", "DEBUGVIEW_BRDF_X", "DEBUGVIEW_SSAO"
	};

	struct RenderFeatures
	{
		bool UseSSAO = true;
		bool UseIBL = true;
		PackNormalsMethod PackNormalsMethod = PackNormalsMethod::None;
		DebugView DebugView = DebugView::None;
	};

	struct PSOEntry
	{
	public:
		std::string Name;
		std::string CurrentKey;
		std::vector<RenderFeatureID> DependentParams;
		std::shared_ptr<PipelineStateBase> Pso;

		std::function<std::shared_ptr<PipelineStateBase>()> BuildPsoFunc;
		std::function<void()> OnPsoUpdated;

	public:
		PSOEntry();
		~PSOEntry();

		PSOEntry(const PSOEntry&) = delete;
		PSOEntry(PSOEntry&&) = delete;
		PSOEntry& operator = (const PSOEntry&) = delete;
		PSOEntry& operator = (PSOEntry&&) = delete;

		void Initialize();

		std::string MakeKeyFromFeatures(const RenderFeatures& f) const;
	};

	class PSOCache
	{
	public:
		void Register(PSOEntry* entry);
		void Unregister(PSOEntry* entry);

		void OnRenderFeaturesChanged(const RenderFeatures& f, RenderFeatureID changed);
		void Clear();

	private:
		void UpdateEntry(PSOEntry& entry, const RenderFeatures& f);
		bool EntryDependsOn(PSOEntry* entry, RenderFeatureID changed);

	private:
		std::vector<PSOEntry*> m_AllEntries;
		std::unordered_map<std::string, std::shared_ptr<PipelineStateBase>> m_Cache;
	};
}