#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "d3dx12.h"

#include <EngineTypes.h>

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#ifdef GRAPHICS_STATIC
#define GRAPHICS_API
#else

#ifdef GRAPHICS_EXPORTS
#define GRAPHICS_API __declspec(dllexport)
#else
#define GRAPHICS_API __declspec(dllimport)
#endif

#endif

static constexpr DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R8G8B8A8_UNORM;