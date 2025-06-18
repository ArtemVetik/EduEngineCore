#pragma once

#if defined(DEBUG) | defined(_DEBUG)
#include "DxException.h"

#include <string>
#include <cstdlib>
#include <iostream>

inline void EnsureStr(const char*) {}

#ifndef THROW_IF_FAILED
#define THROW_IF_FAILED(hr, message)                                                                    \
    do {                                                                                                \
        if (FAILED(hr)) {                                                                               \
            HRESULT hr__ = (hr);                                                                        \
            std::wstring wfn = AnsiToWString(__FILE__);                                                 \
           OutputDebugStringW(DxException::GetHRError(hr__, message, L#hr, wfn, __LINE__).c_str());     \
        }                                                                                               \
    } while(0)
#endif

#ifndef VERIFY_EXPR
#define VERIFY_EXPR(expr, fmt, ...)                         \
    do {                                                    \
        if (!(expr)) {                                      \
            ASSERT_FAILED(fmt, ##__VA_ARGS__);              \
        }                                                   \
    } while (0)
#endif

#ifndef ASSERT_FAILED
#define ASSERT_FAILED(message, ...)                                                              \
    do {                                                                                            \
        EnsureStr(message);                                                                         \
        EduEngine::MsgStream ms;                                                                    \
        EduEngine::FormatMsg(ms, message, __VA_ARGS__);                                           \
        EduEngine::DxException::AssertError(ms.str().c_str(), __FUNCTION__, __FILE__, __LINE__);  \
    } while(0)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(message, ...)                             \
 do {                                                       \
        EnsureStr(message);                                 \
        EduEngine::MsgStream ms;                            \
        ms << "ENGINE ERROR: ";                             \
        EduEngine::FormatMsg(ms, message, __VA_ARGS__);     \
        ms << "\n";                                         \
        OutputDebugStringA(ms.str().c_str());               \
    } while(0)
#endif

#else

#define THROW_IF_FAILED(hr, message)    do {} while(0)
#define VERIFY_EXPR(expr, fmt, ...)     do {} while(0)
#define ASSERT_FAILED(message, ...)     do {} while(0)
#define LOG_ERROR(message, ...)         do {} while(0)

#endif