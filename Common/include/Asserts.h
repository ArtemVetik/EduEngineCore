#pragma once

#if defined(DEBUG) | defined(_DEBUG)
#include "DxException.h"

#include <string>
#include <cstdlib>
#include <iostream>

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

#ifndef VERIFY_EXPR
#define VERIFY_EXPR(expr, message)                                         \
    do {                                                                   \
        if (!(expr)) {                                                     \
            std::cerr << "Verification failed: " << #expr << "\n"          \
                      << "Message: " << message << "\n"                    \
                      << "File: " << __FILE__ << "\n"                      \
                      << "Line: " << __LINE__ << "\n"                      \
                      << "Function: " << __func__ << "\n";                 \
            throw -1;                                                      \
        }                                                                  \
    } while (0)
#endif

#ifndef ASSERT_FAILED
#define ASSERT_FAILED(message) VERIFY_EXPR(0, message)
#endif

#else

#define THROW_IF_FAILED
#define VERIFY_EXPR
#define ASSERT_FAILED

#endif