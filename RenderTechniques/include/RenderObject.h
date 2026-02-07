#pragma once
#include "framework.h"

#include <Mesh.h>
#include <SimpleMath.h>

using namespace DirectX;

namespace EduEngine
{
	struct RENDERTECHNIQUES_API RenderObject
	{
		Mesh* Mesh;
		XMMATRIX World;
	};
}