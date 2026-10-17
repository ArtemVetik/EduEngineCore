#include "VirtualShadowMapsDemoFactory.h"
#include "VirtualShadowMapsDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> VirtualShadowMapsDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<VirtualShadowMapsDemo>();
		demo->StartUp(window);

		return demo;
	}
}