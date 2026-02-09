#pragma once
#include <ReflectionProbe.h>

namespace EduEngine
{
	class SponzaDemo;

	class SponzaGUI
	{
	public:
		SponzaGUI();

		void Init(SponzaDemo* parent);

		void RenderImGUI();

		void DebugDrawReflectionProbes();

	private:
		SponzaDemo* m_Sponza;
		bool m_ActiveReflections[ReflectionProbesManager::MAX_REFLECTION_PROBES];
	};
}