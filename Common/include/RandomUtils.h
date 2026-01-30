#pragma once
#include <cstdlib>

namespace EduEngine
{
	__forceinline float RandF()
	{
		return (float)(rand()) / (float)RAND_MAX;
	}

	__forceinline float RandF(float a, float b)
	{
		return a + RandF() * (b - a);
	}
}