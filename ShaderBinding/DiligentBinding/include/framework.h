#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "d3dx12.h"

#include "../../../Common/include/EngineTypes.h"

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef DILIGENTBINDING_STATIC
#define DILIGENTBINDING_API
#else

#ifdef DILIGENTBINDING_EXPORTS
#define DILIGENTBINDING_API __declspec(dllexport)
#else
#define DILIGENTBINDING_API __declspec(dllimport)
#endif

#endif