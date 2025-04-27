#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

#include "../../Common/include/EngineTypes.h"

#ifdef GRAPHICSHEAPS_STATIC
#define GRAPHICS_HEAPS_API
#else

#ifdef GRAPHICSHEAPS_EXPORTS
#define GRAPHICS_HEAPS_API __declspec(dllexport)
#else
#define GRAPHICS_HEAPS_API __declspec(dllimport)
#endif

#endif