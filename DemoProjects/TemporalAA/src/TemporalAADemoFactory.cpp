#include "TemporalAADemoFactory.h"
#include "TemporalAADemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> TemporalAADemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<TemporalAADemo>();
		demo->StartUp(window);

		return demo;
	}
}