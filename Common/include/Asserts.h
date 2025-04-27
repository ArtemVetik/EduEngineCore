#pragma once

#if defined(DEBUG) | defined(_DEBUG)
#include <string>
#include "DxException.h"

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef THROW_IF_FAILED
#define THROW_IF_FAILED(expr, message)                                \
    do {                                                              \
        if (FAILED(expr)) {                                           \
            HRESULT hr__ = (expr);                                    \
            std::wstring wfn = AnsiToWString(__FILE__);               \
            DxException exc(hr__, message, L#expr, wfn, __LINE__);    \
            OutputDebugStringW(exc.ToString().c_str());               \
            throw exc;                                                \
        }                                                             \
    } while(false)
#endif

#ifndef ASSERT_FAILED
#define ASSERT_FAILED(message) THROW_IF_FAILED(0, message)
#endif

#else

#define THROW_IF_FAILED
#define ASSERT_FAILED

#endif