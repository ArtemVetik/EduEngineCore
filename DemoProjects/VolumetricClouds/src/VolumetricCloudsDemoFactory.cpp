#include "VolumetricCloudsDemoFactory.h"
#include "VolumetricCloudsDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> VolumetricCloudsDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<VolumetricCloudsDemo>();
		demo->StartUp(window);

		return demo;
	}
}