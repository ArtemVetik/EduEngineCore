#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <d3d12.h>

#ifdef RENDERTECHNIQUES_EXPORTS
#define RENDERTECHNIQUES_API __declspec(dllexport)
#else
#define RENDERTECHNIQUES_API __declspec(dllimport)
#endif