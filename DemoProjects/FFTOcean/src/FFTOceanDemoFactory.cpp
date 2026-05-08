#include "FFTOceanDemoFactory.h"
#include "FFTOceanDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> FFTOceanDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<FFTOceanDemo>();
		demo->StartUp(window);

		return demo;
	}
}