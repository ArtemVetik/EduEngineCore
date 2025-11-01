#include "PBRDemoFactory.h"
#include "PBRDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> PBRDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<PBRDemo>();
		demo->StartUp(window);

		return demo;
	}
}