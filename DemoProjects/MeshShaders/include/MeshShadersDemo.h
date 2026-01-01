#pragma once

#include <RenderEngine.h>

using namespace EduEngine::EduBinding;

namespace EduEngine
{
	class MeshShadersDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;
	};
}