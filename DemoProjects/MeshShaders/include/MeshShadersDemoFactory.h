#pragma once

#include <IRenderEngine.h>

namespace EduEngine
{
	class MeshShadersDemoFactory
	{
	public:
		static std::shared_ptr<IRenderEngine> Create(const Window& window);
	};
}