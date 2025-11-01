#pragma once

#include "../../BaseDemo/include/IRenderEngine.h"

namespace EduEngine
{
	class PBRDemoFactory
	{
	public:
		static std::shared_ptr<IRenderEngine> Create(const Window& window);
	};
}