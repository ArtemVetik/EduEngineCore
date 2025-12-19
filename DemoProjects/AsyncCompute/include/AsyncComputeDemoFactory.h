#pragma once

#include <IRenderEngine.h>

namespace EduEngine
{
	class AsyncComputeDemoFactory
	{
	public:
		static std::shared_ptr<IRenderEngine> Create(const Window& window);
	};
}