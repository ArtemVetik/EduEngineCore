#pragma once

#ifdef INPUTSYSTEM_STATIC
#define INPUTSYSTEM_API
#else

#ifdef INPUTSYSTEM_EXPORTS
#define INPUTSYSTEM_API __declspec(dllexport)
#else
#define INPUTSYSTEM_API __declspec(dllimport)
#endif

#endif