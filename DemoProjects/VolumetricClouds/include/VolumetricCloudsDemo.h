#pragma once
#include <RenderEngine.h>

namespace EduEngine
{
	class VolumetricCloudsDemo : public RenderEngine
	{
	public:
		void ChangeInitInfo(EngineInitInfo& info) override;
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;
	};
}