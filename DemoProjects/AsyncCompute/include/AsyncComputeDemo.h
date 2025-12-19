#pragma once

#include <RenderEngine.h>

namespace EduEngine
{
	class AsyncComputeDemo : public RenderEngine
	{
	protected:
		void OnStartUp() override;
		void OnUpdate(const Timer& timer) override;
		void OnRender(const Timer& timer) override;

	private:
	};
}