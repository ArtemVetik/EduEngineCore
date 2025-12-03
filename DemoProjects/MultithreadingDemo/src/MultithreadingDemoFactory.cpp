#include "MultithreadingDemoFactory.h"
#include "MultithreadingDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> MultithreadingDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<MultithreadingDemo>();
		demo->StartUp(window);

		return demo;
	}
}