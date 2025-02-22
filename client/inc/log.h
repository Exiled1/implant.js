#pragma once

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

#ifdef LINUX
#include <dlfcn.h>
#endif

#include "winhdr.h"

namespace Log {
    void Init();
    void _Debug(const char *filename, int linenum, const char *func, const char *fmt...);
    void _Debug_Errno(const char *filename, int linenum, const char *func, const char *fmt...);
    void _Info(const char *filename, int linenum, const char *func, const char *fmt...);
    void _Warn(const char *filename, int linenum, const char *func, const char *fmt...);
    void _Error(const char *filename, int linenum, const char *func, const char *fmt...);
    void _Error_Errno(const char *filename, int linenum, const char *func, const char *fmt...);
#ifdef LINUX
    void _Error_DlErrno(const char *filename, int linenum, const char *func, const char *fmt...);
#endif
#ifdef WINDOWS
    void _Error_WsaErrno(const char *filename, int linenum, const char *func, const char *fmt...);
#endif
}

#ifdef DEBUG_BUILD
#define LOG_DEBUG(fmt, ...) Log::_Debug(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG_ERRNO(fmt, ...) Log::_Debug_Errno(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) do {} while (0);
#define LOG_DEBUG_ERRNO(fmt, ...) do {} while (0);
#endif

#define LOG_INFO(fmt, ...) Log::_Info(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) Log::_Warn(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Log::_Error(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR_ERRNO(fmt, ...) Log::_Error_Errno(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#ifdef LINUX
#define LOG_ERROR_FFI(fmt, ...) Log::_Error_DlErrno(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define LOG_ERROR_NET(fmt, ...) LOG_ERROR_ERRNO(fmt, ##__VA_ARGS__)
#endif

#ifdef WINDOWS
#define LOG_ERROR_FFI(fmt, ...) LOG_ERROR_ERRNO(fmt, ##__VA_ARGS__)
#define LOG_ERROR_NET(fmt, ...) Log::_Error_WsaErrno(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)
#endif