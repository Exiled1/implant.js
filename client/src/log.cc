#include "log.h"

#define BUF_SIZE 4096
#define TS_SIZE 32

#define RESET "\x1b[0m"
#define RED "\x1b[1;31m"
#define GREEN "\x1b[1;32m"
#define YELLOW "\x1b[1;33m"
#define BLUE "\x1b[1;34m"
#define GRAY "\x1b[38;5;244m"

static void _log_msg(const char *prefix, const char *filename, int linenum, const char *func, char *err, const char *fmt, va_list args) {
    char msg[BUF_SIZE] = {0};
    char ts[TS_SIZE] = {0};

    time_t curtime = time(nullptr);
    struct tm *lt = localtime(&curtime);
    strftime(ts, TS_SIZE, "%Y-%m-%d %H:%M:%S", lt);

    vsnprintf(msg, BUF_SIZE-1, fmt, args);

    if (err) {
        fprintf(stderr, "[%s][" GRAY "%s:%d!%s" RESET "][%s] %s (error: %s)\n", ts, filename, linenum, func, prefix, msg, err);
#ifdef WINDOWS
        LocalFree(err);
#endif
    } else {
        fprintf(stderr, "[%s][" GRAY "%s:%d!%s" RESET "][%s] %s\n", ts, filename, linenum, func, prefix, msg);
    }

    fflush(stderr);
}

#ifdef WINDOWS
static char *_get_win_errstr(DWORD errnum) {
    char *err;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errnum,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&err,
        0,
        NULL
    );

    // remove trailing \r\n
    err[strcspn(err, "\r")] = '\0';
    err[strcspn(err, "\n")] = '\0';

    return err;
}
#endif

static char *_get_normal_error() {
#ifdef LINUX
    return strerror(errno);
#endif
#ifdef WINDOWS
    return _get_win_errstr(GetLastError());
#endif

    return nullptr;
}

namespace Log {
    void Init() {
#ifdef LINUX
        setvbuf(stdin, nullptr, 2, 0);
        setvbuf(stdout, nullptr, 2, 0);
        setvbuf(stderr, nullptr, 2, 0);
#endif
    }
    
    void _Debug(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        _log_msg(BLUE ">" RESET, filename, linenum, func, nullptr, fmt, args);
        va_end(args);
    }

    void _Debug_Errno(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        char *err = _get_normal_error();
        _log_msg(BLUE ">" RESET, filename, linenum, func, err, fmt, args);
        va_end(args);
    }

    void _Info(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        _log_msg(GREEN "+" RESET, filename, linenum, func, nullptr, fmt, args);
        va_end(args);
    }

    void _Warn(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        _log_msg(YELLOW "*" RESET, filename, linenum, func, nullptr, fmt, args);
        va_end(args);
    }

    void _Error(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        _log_msg(RED "!" RESET, filename, linenum, func, nullptr, fmt, args);
        va_end(args);
    }

    void _Error_Errno(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        char *err = _get_normal_error();
        _log_msg(RED "!" RESET, filename, linenum, func, err, fmt, args);
        va_end(args);
    }

#ifdef LINUX
    void _Error_DlErrno(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        char *err = dlerror();
        _log_msg(RED "!" RESET, filename, linenum, func, err, fmt, args);
        va_end(args);
    }
#endif

#ifdef WINDOWS
    void _Error_WsaErrno(const char *filename, int linenum, const char *func, const char *fmt...) {
        va_list args;
        va_start(args, fmt);
        char *err = _get_win_errstr(WSAGetLastError());
        _log_msg(RED "!" RESET, filename, linenum, func, err, fmt, args);
        va_end(args);
    }
#endif
}