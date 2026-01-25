#pragma once

#include <IRenderEngine.h>

namespace EduEngine
{
	class TemporalAADemoFactory
	{
	public:
		static std::shared_ptr<IRenderEngine> Create(const Window& window);
	};
}