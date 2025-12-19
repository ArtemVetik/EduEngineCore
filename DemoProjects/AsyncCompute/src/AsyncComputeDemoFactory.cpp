#include "AsyncComputeDemoFactory.h"
#include "AsyncComputeDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> AsyncComputeDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<AsyncComputeDemo>();
		demo->StartUp(window);

		return demo;
	}
}