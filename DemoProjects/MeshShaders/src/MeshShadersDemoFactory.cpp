#include "MeshShadersDemoFactory.h"
#include "MeshShadersDemo.h"

namespace EduEngine
{
	std::shared_ptr<IRenderEngine> MeshShadersDemoFactory::Create(const Window& window)
	{
		auto demo = std::make_shared<MeshShadersDemo>();
		demo->StartUp(window);

		return demo;
	}
}