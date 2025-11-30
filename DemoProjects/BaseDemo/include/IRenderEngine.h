#pragma once

#include <memory>

#include "framework.h"
#include "Window.h"

#include <Timer.h>

namespace EduEngine
{
	class IRenderEngine
	{
	public:
		virtual void Update(const Timer& timer) = 0;
		virtual void Render(const Timer& timer) = 0;
		
		static std::shared_ptr<IRenderEngine> Create(const Window& mainWindow);
	};
}