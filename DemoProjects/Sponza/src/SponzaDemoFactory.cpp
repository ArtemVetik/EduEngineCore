#include "SponzaDemoFactory.h"
#include "SponzaDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> SponzaDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<SponzaDemo>();
		demo->StartUp(window);

		return demo;
	}
}