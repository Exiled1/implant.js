#pragma once

#include <libplatform/libplatform.h>
#include <v8.h>

#ifdef LINUX
#include <stdio.h>
#endif

#include "consts.h"
#include "log.h"
#include "state.h"
#include "utils.h"

#define JS_HANDLER(func) void func(const v8::FunctionCallbackInfo<v8::Value>& info)

#define JS_CONST(sym) void const_##sym(const v8::FunctionCallbackInfo<v8::Value>& info)

// layout of this should mirror modules/types.d.ts
namespace JsNatives {
    JS_HANDLER(output);
    JS_HANDLER(system);
    JS_HANDLER(os);

    namespace mem {
        JS_HANDLER(alloc);
        JS_HANDLER(free);
        JS_HANDLER(read);
        JS_HANDLER(read_dword);
        JS_HANDLER(read_qword);
        JS_HANDLER(write);
        JS_HANDLER(write_dword);
        JS_HANDLER(write_qword);
        JS_HANDLER(copy);
        JS_HANDLER(equal);
    }

    namespace fs {
        JS_HANDLER(open);
        JS_HANDLER(close);
        JS_HANDLER(read);
        JS_HANDLER(read_line);
        JS_HANDLER(read_all);
        JS_HANDLER(write);
        JS_HANDLER(seek);
        JS_HANDLER(eof);
        JS_HANDLER(delete_file);
        JS_HANDLER(file_exists);
        JS_HANDLER(dir_exists);
        JS_HANDLER(dir_contents);
    }

    namespace ffi {
        JS_HANDLER(resolve);
        JS_HANDLER(define);
    }

    // unmapped namespace used to define getters for the consts in types.d.ts
    namespace consts {
        JS_CONST(MEM_RW);
        JS_CONST(MEM_RWX);

        JS_CONST(MODE_R);
        JS_CONST(MODE_W);
        JS_CONST(MODE_RW);

        JS_CONST(SEEK_SET);
        JS_CONST(SEEK_END);
        JS_CONST(SEEK_CUR);

        JS_CONST(TYPE_VOID);
        JS_CONST(TYPE_INTEGER);
        JS_CONST(TYPE_POINTER);
        JS_CONST(TYPE_BOOL);
        JS_CONST(TYPE_STRING);

        JS_CONST(OS_LINUX);
        JS_CONST(OS_WINDOWS);
    }
}