#include "jsnatives.h"

#define V8_STR(str) v8::String::NewFromUtf8(iso, str).ToLocalChecked()

#define THROW_EXC(exc, msg) iso->ThrowException(exc(V8_STR(msg)));
#define THROW_ERROR(msg, ...) THROW_EXC(v8::Exception::Error, Utils::format_string(msg, ##__VA_ARGS__).c_str())
#define THROW_FFI_ERROR(msg, ...) THROW_ERROR("ffi error for %s: " msg, ff->name().c_str(), ##__VA_ARGS__)

// tl;dr try to convert a value to its local version. if it fails, log an error in the client,
// and propagate an exception into javascript to be handled.
#define TO_LOCAL_RET(local, maybe, err, ret) \
    { \
        v8::Local<v8::Value> *jsexc = nullptr; \
        { \
            v8::TryCatch trycatch(iso); \
            if (!maybe.ToLocal(&local)) { \
                auto exc(trycatch.Exception()); \
                LOG_ERROR(err ": %s", *v8::String::Utf8Value(iso, exc)); \
                jsexc = new v8::Local<v8::Value>(v8::Exception::TypeError(exc->ToString(ctx).ToLocalChecked())); \
            } \
        } \
        if (jsexc) { \
            iso->ThrowException(*jsexc); \
            delete jsexc; \
            return ret; \
        } \
    }

#define NO_RET
#define TO_LOCAL(local, maybe, err) TO_LOCAL_RET(local, maybe, err, NO_RET)

#define ARG_UINT8ARR(param, var) var = param.As<v8::Uint8Array>();

#define ARG_INT32(param, var) \
    { \
        v8::Local<v8::Int32> local; \
        TO_LOCAL(local, param->ToInt32(ctx), "failed to get " #var); \
        var = local->Value(); \
    }

#define ARG_UINT32(param, var) \
    { \
        v8::Local<v8::Uint32> local; \
        TO_LOCAL(local, param->ToUint32(ctx), "failed to get " #var); \
        var = local->Value(); \
    }

#define ARG_NUMBER(param, var) \
    { \
        v8::Local<v8::Number> local; \
        TO_LOCAL(local, param->ToNumber(ctx), "failed to get " #var); \
        var = local->Value(); \
    }

#define ARG_UINT64(param, var) \
    { \
        v8::Local<v8::BigInt> local_##var; \
        TO_LOCAL(local_##var, param->ToBigInt(ctx), "failed to get " #var); \
        var = local_##var->Uint64Value(); \
    }

#define ARG_STR(param, var) \
    { \
        v8::String::Utf8Value v8s(iso, param); \
        var = std::string(*v8s); \
    }

#define ARG_BOOL(param, var) var = param->BooleanValue(iso)

#define JS_CONST_IMPL(sym) \
    void JsNatives::consts::const_##sym(const v8::FunctionCallbackInfo<v8::Value>& info) { \
        info.GetReturnValue().Set(CONST_##sym); \
    }

JS_CONST_IMPL(MEM_RW);
JS_CONST_IMPL(MEM_RWX);

JS_CONST_IMPL(MODE_R);
JS_CONST_IMPL(MODE_W);
JS_CONST_IMPL(MODE_RW);

JS_CONST_IMPL(SEEK_SET);
JS_CONST_IMPL(SEEK_END);
JS_CONST_IMPL(SEEK_CUR);

JS_CONST_IMPL(TYPE_VOID);
JS_CONST_IMPL(TYPE_INTEGER);
JS_CONST_IMPL(TYPE_POINTER);
JS_CONST_IMPL(TYPE_BOOL);
JS_CONST_IMPL(TYPE_STRING);

JS_CONST_IMPL(OS_LINUX);
JS_CONST_IMPL(OS_WINDOWS);

JS_HANDLER(JsNatives::output) {
    auto iso = info.GetIsolate();

    if (info.Length() != 1 || !info[0]->IsString()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.output()");
        return;
    }
    std::string msg;
    ARG_STR(info[0], msg);

    STATE->add_output(msg);
}

JS_HANDLER(JsNatives::system) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() < 1 || info.Length() > 2) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.system()");
        return;
    }

    std::string cmd;
    ARG_STR(info[0], cmd);

    bool ignore_status = false;
    if (info.Length() == 2) {
        ARG_BOOL(info[1], ignore_status);
    }

    int status;
    std::string* ret = Utils::run_cmd(cmd, &status);
    if (ret == nullptr) {
        THROW_ERROR("failed to get cmd output");
        return;
    }

    if (!ignore_status && status > 0) {
        THROW_ERROR("cmd had nonzero return status");
        return;
    }

    v8::Local<v8::Value> local_output;
    TO_LOCAL(local_output, v8::String::NewFromUtf8(iso, ret->c_str()), "failed to parse output string to v8");
    delete ret;

    info.GetReturnValue().Set(local_output);
}

JS_HANDLER(JsNatives::os) {
#ifdef LINUX
    info.GetReturnValue().Set(CONST_OS_LINUX);
    return;
#endif

#ifdef WINDOWS
    info.GetReturnValue().Set(CONST_OS_WINDOWS);
    return;
#endif
}

JS_HANDLER(JsNatives::mem::alloc) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2 || !info[0]->IsNumber() || !info[1]->IsNumber()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.alloc()");
        return;
    }

    double sz, perms;
    ARG_NUMBER(info[0], sz);
    ARG_NUMBER(info[1], perms);

    auto ptr = STATE->mem_alloc((uint32_t)sz, (uint32_t)perms);
    if (!ptr) {
        THROW_ERROR("failed to alloc memory");
        return;
    }

    v8::Local<v8::BigInt> ret(v8::BigInt::NewFromUnsigned(iso, (uint64_t)ptr));
    info.GetReturnValue().Set(ret);
}

JS_HANDLER(JsNatives::mem::free) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsBigInt()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.free()");
        return;
    }

    uint64_t ptr;
    ARG_UINT64(info[0], ptr);

    if (!STATE->mem_free((void*)ptr)) {
        THROW_ERROR("failed to free memory");
        return;
    }
}

JS_HANDLER(JsNatives::mem::read) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2 || !info[0]->IsBigInt() || !info[1]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.read()");
        return;
    }

    uint64_t ptr;
    ARG_UINT64(info[0], ptr);
    uint32_t sz;
    ARG_UINT32(info[1], sz);

    if (!ptr) {
        THROW_ERROR("null ptr passed to ctx.mem.read()");
        return;
    }

    auto ab = v8::ArrayBuffer::New(iso, sz);
    ::memcpy(ab->Data(), (void*)ptr, sz);

    auto a = v8::Uint8Array::New(ab, 0, sz);

    info.GetReturnValue().Set(a);
}

JS_HANDLER(JsNatives::mem::read_dword) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsBigInt()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.read_dword()");
        return;
    }

    uint64_t ptr;
    ARG_UINT64(info[0], ptr);

    if (!ptr) {
        THROW_ERROR("null ptr passed to ctx.mem.read_dword()");
        return;
    }

    info.GetReturnValue().Set(v8::Uint32::New(iso, *(uint32_t*)ptr)->ToNumber(ctx).ToLocalChecked());
}

JS_HANDLER(JsNatives::mem::read_qword) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsBigInt()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.read_qword()");
        return;
    }

    uint64_t ptr;
    ARG_UINT64(info[0], ptr);

    if (!ptr) {
        THROW_ERROR("null ptr passed to ctx.mem.read_qword()");
        return;
    }

    info.GetReturnValue().Set(v8::BigInt::New(iso, *(uint64_t*)ptr));
}

JS_HANDLER(JsNatives::mem::write) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2 || !info[0]->IsBigInt() || !info[1]->IsUint8Array()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.write()");
        return;
    }

    uint64_t ptr;
    ARG_UINT64(info[0], ptr);
    v8::Local<v8::Uint8Array> arr;
    ARG_UINT8ARR(info[1], arr);

    if (!ptr) {
        THROW_ERROR("null ptr passed to ctx.mem.write()");
        return;
    }

    ::memcpy((void*)ptr, arr->Buffer()->Data(), arr->Length());
}

JS_HANDLER(JsNatives::mem::write_dword) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2 || !info[0]->IsBigInt() || !info[1]->IsNumber()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.write_dword()");
        return;
    }

    uint64_t ptr;
    ARG_UINT64(info[0], ptr);
    double num;
    ARG_NUMBER(info[1], num);

    if (!ptr) {
        THROW_ERROR("null ptr passed to ctx.mem.write_dword()");
        return;
    }

    *(uint32_t*)ptr = (uint32_t)num;
}

JS_HANDLER(JsNatives::mem::write_qword) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2 || !info[0]->IsBigInt() || !info[1]->IsBigInt()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.write_qword()");
        return;
    }

    uint64_t ptr, num;
    ARG_UINT64(info[0], ptr);
    ARG_UINT64(info[1], num);

    if (!ptr) {
        THROW_ERROR("null ptr passed to ctx.mem.write_qword()");
        return;
    }

    *(uint64_t*)ptr = num;
}

JS_HANDLER(JsNatives::mem::copy) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 3 || !info[0]->IsBigInt() || !info[1]->IsBigInt() || !info[2]->IsNumber()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.copy()");
        return;
    }

    uint64_t dst, src;
    ARG_UINT64(info[0], dst);
    ARG_UINT64(info[1], src);
    double size;
    ARG_NUMBER(info[2], size);

    if (!dst || !src) {
        THROW_ERROR("null ptr passed to ctx.mem.copy()");
        return;
    }

    ::memcpy((void*)dst, (void*)src, (size_t)size);
}

JS_HANDLER(JsNatives::mem::equal) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() < 2) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.equal(): not enough args");
        return;
    }

    uint32_t* sz = nullptr;
    if (info.Length() == 3) {
        // validate the size arg
        if (!info[2]->IsUint32()) {
            THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.equal(): invalid size param");
            return;
        }

        sz = new uint32_t;
        ARG_UINT32(info[2], *sz);
    }

    // get a pointer to both data chunks, and set the size if need be
    uint64_t ptr1, ptr2;
    v8::Local<v8::Uint8Array> arr1, arr2;
    if (info[0]->IsBigInt()) {
        ARG_UINT64(info[0], ptr1);
    }
    else if (info[0]->IsUint8Array()) {
        ARG_UINT8ARR(info[0], arr1);
        ptr1 = (uint64_t)arr1->Buffer()->Data();

        if (!sz) {
            sz = new uint32_t;
            *sz = arr1->Length();
        }
    }
    else {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.equal(): invalid data1");
        return;
    }

    if (info[1]->IsBigInt()) {
        ARG_UINT64(info[1], ptr2);
    }
    else if (info[1]->IsUint8Array()) {
        ARG_UINT8ARR(info[1], arr2);
        ptr2 = (uint64_t)arr2->Buffer()->Data();

        if (!sz) {
            sz = new uint32_t;
            *sz = arr2->Length();
        }
        else if (info.Length() && *sz > arr2->Length()) {
            // only the smallest size should be set if not overridden
            *sz = arr2->Length();
        }
    }
    else {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.equal(): invalid data2");
        return;
    }

    if (!sz) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.equal(): no size found");
        return;
    }

    // make sure the overridden size is not out of bounds
    if (info.Length() == 3 && (
        (info[0]->IsUint8Array() && *sz > arr1->Length()) ||
        (info[1]->IsUint8Array() && *sz > arr2->Length())
        )
        ) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.mem.equal(): size is bigger than a provided array");
        return;
    }

    bool eq = ::memcmp((void*)ptr1, (void*)ptr2, *sz) == 0;

    info.GetReturnValue().Set(eq);
}

JS_HANDLER(JsNatives::fs::open) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2 || !info[0]->IsString() || !info[1]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.open()");
        return;
    }

    std::string path;
    ARG_STR(info[0], path);
    uint32_t mode;
    ARG_UINT32(info[1], mode);

    jshandle_t h = STATE->open_file(path, mode);
    if (h == INVALID_HANDLE) {
        THROW_ERROR("failed to open file");
        return;
    }

    info.GetReturnValue().Set(h);
}

JS_HANDLER(JsNatives::fs::close) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.close()");
        return;
    }

    jshandle_t handle;
    ARG_UINT32(info[0], handle);

    if (!STATE->close_file(handle)) {
        THROW_ERROR("failed to close file");
        return;
    }
}

JS_HANDLER(JsNatives::fs::read) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2 || !info[0]->IsUint32() || !info[1]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.read()");
        return;
    }

    jshandle_t handle;
    ARG_UINT32(info[0], handle);
    std::size_t sz;
    ARG_UINT32(info[1], sz);

    std::vector<char>* data = STATE->read_file(handle, sz);
    if (!data) {
        THROW_ERROR("failed to read from file");
        return;
    }

    auto ab = v8::ArrayBuffer::New(iso, sz);
    ::memcpy(ab->Data(), (void*)data->data(), sz);
    delete data;

    auto a = v8::Uint8Array::New(ab, 0, sz);

    info.GetReturnValue().Set(a);
}

JS_HANDLER(JsNatives::fs::read_line) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.read_line()");
        return;
    }

    jshandle_t handle;
    ARG_UINT32(info[0], handle);

    std::string* line = STATE->read_line(handle);
    if (!line) {
        THROW_ERROR("failed to read line from file");
        return;
    }

    v8::Local<v8::Value> local_output;
    TO_LOCAL(local_output, v8::String::NewFromUtf8(iso, line->c_str()), "failed to parse string to v8");
    delete line;

    info.GetReturnValue().Set(local_output);
}

JS_HANDLER(JsNatives::fs::read_all) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.read_all()");
        return;
    }

    jshandle_t handle;
    ARG_UINT32(info[0], handle);

    std::vector<char>* data = STATE->read_all(handle);
    if (!data) {
        THROW_ERROR("failed to read from file");
        return;
    }

    std::size_t sz = data->size();

    auto ab = v8::ArrayBuffer::New(iso, sz);
    ::memcpy(ab->Data(), (void*)data->data(), sz);
    delete data;

    auto a = v8::Uint8Array::New(ab, 0, sz);

    info.GetReturnValue().Set(a);
}

JS_HANDLER(JsNatives::fs::write) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 2) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.write(): not enough args");
        return;
    }

    if (!info[0]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.write(): invalid handle");
        return;
    }
    jshandle_t handle;
    ARG_UINT32(info[0], handle);

    std::vector<char> v;
    if (info[1]->IsString()) {
        std::string data_str;
        ARG_STR(info[1], data_str);

        v = std::vector<char>(data_str.begin(), data_str.end());
    }
    else if (info[1]->IsUint8Array()) {
        v8::Local<v8::Uint8Array> data_arr;
        ARG_UINT8ARR(info[1], data_arr);

        std::size_t sz = data_arr->Length();
        uint8_t* ptr = (uint8_t*)data_arr->Buffer()->Data();
        v = std::vector<char>(ptr, ptr + sz);
    }
    else {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.write(): invalid data");
        return;
    }

    if (!STATE->write_file(handle, &v)) {
        THROW_ERROR("failed to write to file");
        return;
    }
}

JS_HANDLER(JsNatives::fs::seek) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 3 || !info[0]->IsUint32() || !info[1]->IsUint32() || !info[2]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.seek()");
        return;
    }

    jshandle_t handle;
    uint32_t offset, whence;
    ARG_UINT32(info[0], handle);
    ARG_UINT32(info[1], offset);
    ARG_UINT32(info[2], whence);

    if (!STATE->seek_file(handle, offset, whence)) {
        THROW_ERROR("failed to seek in file");
        return;
    }
}

JS_HANDLER(JsNatives::fs::eof) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsUint32()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.eof()");
        return;
    }

    jshandle_t handle;
    ARG_UINT32(info[0], handle);

    info.GetReturnValue().Set(STATE->eof(handle));
}

JS_HANDLER(JsNatives::fs::delete_file) {
    auto iso = info.GetIsolate();

    if (info.Length() != 1 || !info[0]->IsString()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.delete_file()");
        return;
    }
    std::string path;
    ARG_STR(info[0], path);

    if (!Utils::Fs::delete_file(path)) {
        THROW_ERROR("failed to delete file");
        return;
    }
}

JS_HANDLER(JsNatives::fs::file_exists) {
    auto iso = info.GetIsolate();

    if (info.Length() != 1 || !info[0]->IsString()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.file_exists()");
        return;
    }
    std::string path;
    ARG_STR(info[0], path);

    info.GetReturnValue().Set(Utils::Fs::file_exists(path));
}

JS_HANDLER(JsNatives::fs::dir_exists) {
    auto iso = info.GetIsolate();

    if (info.Length() != 1 || !info[0]->IsString()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.dir_exists()");
        return;
    }
    std::string path;
    ARG_STR(info[0], path);

    info.GetReturnValue().Set(Utils::Fs::dir_exists(path));
}

JS_HANDLER(JsNatives::fs::dir_contents) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() != 1 || !info[0]->IsString()) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.fs.is_file()");
        return;
    }
    std::string path;
    ARG_STR(info[0], path);

    auto dir_ents = Utils::Fs::dir_contents(path);
    if (!dir_ents) {
        THROW_ERROR("failed to enumerate directory contents in %s", path.c_str());
        return;
    }

    auto ret = v8::Array::New(iso, dir_ents->size());
    for (unsigned int i = 0; i < dir_ents->size(); i++) {
        v8::Local<v8::String> s;
        auto maybe_s = v8::String::NewFromUtf8(iso, dir_ents->at(i).c_str());
        TO_LOCAL(s, maybe_s, "couldn't convert dir entry to v8 string");

        ret->Set(ctx, i, s).Check();
    }

    info.GetReturnValue().Set(ret);
}

std::vector<FfiType>* get_ffi_types(v8::Isolate* iso, v8::Local<v8::Context>& ctx, v8::Local<v8::Value> arg) {
    if (!arg->IsArray()) {
        THROW_EXC(v8::Exception::TypeError, "invalid ffi argument types");
        return nullptr;
    }

    std::vector<FfiType>* arg_types = new std::vector<FfiType>;
    v8::Local<v8::Array> arr = arg.As<v8::Array>();
    for (unsigned int i = 0; i < arr->Length(); i++) {
        v8::MaybeLocal<v8::Value> maybe_element = arr->Get(ctx, i);
        v8::Local<v8::Value> element;

        TO_LOCAL_RET(element, maybe_element, "couldn't parse ffi argument type", nullptr);

        if (!element->IsNumber()) {
            THROW_EXC(v8::Exception::TypeError, "invalid ffi argument type");
        }

        v8::Local<v8::Number> v8_num = element.As<v8::Number>();

        uint32_t arg_type_num = (uint32_t)v8_num->Value();

        FfiType arg_type = Utils::Ffi::type_from_int(arg_type_num);
        if (arg_type == TYPE_INVALID) {
            THROW_EXC(v8::Exception::TypeError, "invalid ffi argument type");
            goto bail;
        }

        arg_types->push_back(arg_type);
    }

    return arg_types;

bail:
    if (arg_types) delete arg_types;
    return nullptr;
}

JS_HANDLER(ffi_callback) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    auto handle_local = v8::Local<v8::Integer>::Cast(info.Data());
    jshandle_t handle;
    if (!handle_local->Uint32Value(ctx).To(&handle)) {
        THROW_ERROR("ffi callback function has an invalid handle");
        return;
    }

    ForeignFunc* ff = STATE->get_function(handle);
    if (!ff) {
        THROW_ERROR("no function available for this handle");
        return;
    }

    // validate there are the expected number of args
    if ((unsigned int)info.Length() != ff->arg_types.size()) {
        THROW_FFI_ERROR("wrong number of arguments, need %d", ff->arg_types.size());
        return;
    }

    // convert every param into a uint64_t
    std::vector<uint64_t> args;
    args.reserve(ff->arg_types.size());
    std::vector<std::string*> allocd_strs;
    for (unsigned int i = 0; i < ff->arg_types.size(); i++) {
        FfiType t = ff->arg_types[i];
        auto v8_arg = info[i];
        switch (t) {
            // a pointer is effectively an integer, can fall through to the TYPE_INTEGER handler as long as its a bigint
        case TYPE_POINTER: {
            if (!v8_arg->IsBigInt()) {
                THROW_FFI_ERROR("an invalid TYPE_POINTER value was provided at idx %d", i);
                goto cleanup;
            }
        }
        case TYPE_INTEGER: {
            // need to handle signed and unsigned, 32bit and 64bit
            if (v8_arg->IsUint32()) {
                uint32_t v;
                ARG_UINT32(v8_arg, v);
                args.push_back(v);
            }
            else if (v8_arg->IsInt32()) {
                int32_t v;
                ARG_INT32(v8_arg, v);
                args.push_back(v); // TODO: make sure this works correctly
            }
            else if (v8_arg->IsBigInt()) {
                uint64_t v;
                ARG_UINT64(v8_arg, v);
                args.push_back(v);
            }
            else {
                THROW_FFI_ERROR("an invalid TYPE_INTEGER value was provided at idx %d", i);
                goto cleanup;
            }
            break;
        }
        case TYPE_BOOL: {
            if (!v8_arg->IsBoolean()) {
                THROW_FFI_ERROR("an invalid TYPE_BOOL value was provided at idx %d", i);
                goto cleanup;
            }
            bool v;
            ARG_BOOL(v8_arg, v);
            args.push_back((uint64_t)v);
            break;
        }
        case TYPE_STRING: {
            if (!v8_arg->IsString()) {
                THROW_FFI_ERROR("an invalid TYPE_STRING value was provided at idx %d", i);
                goto cleanup;
            }
            std::string* s = new std::string();
            ARG_STR(v8_arg, *s);
            args.push_back((uint64_t)s->data());
            allocd_strs.push_back(s);
            break;
        }
        case TYPE_VOID:
        case TYPE_INVALID: {
            LOG_ERROR("wha... how... idk, ur on ur own))");
            THROW_FFI_ERROR("you fool! i have been trained in your ffi arts by count dooku!");
            goto cleanup;
        }
        }
    }

    // ensure enough arguments got parsed
    if (args.size() != ff->arg_types.size()) {
        THROW_FFI_ERROR("didnt parse enough args, need %d", ff->arg_types.size());
        goto cleanup;
    }

    LOG_DEBUG("executing native func via ffi: %s @ %p", ff->name().c_str(), ff->ptr);

    // call func
    // this is very stupid but i am lazy))
    // can prob use some preprocessor magic like https://stackoverflow.com/a/16994140 to clean this up
    uint64_t ret;
    switch (args.size()) {
    case 0:
        ret = ((uint64_t(*)())(ff->ptr))();
        break;
    case 1:
        ret = ((uint64_t(*)(uint64_t))(ff->ptr))(args[0]);
        break;
    case 2:
        ret = ((uint64_t(*)(uint64_t, uint64_t))(ff->ptr))(args[0], args[1]);
        break;
    case 3:
        ret = ((uint64_t(*)(uint64_t, uint64_t, uint64_t))(ff->ptr))(args[0], args[1], args[2]);
        break;
    case 4:
        ret = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t))(ff->ptr))(args[0], args[1], args[2], args[3]);
        break;
    case 5:
        ret = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(ff->ptr))(args[0], args[1], args[2], args[3], args[4]);
        break;
    case 6:
        ret = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(ff->ptr))(args[0], args[1], args[2], args[3], args[4], args[5]);
        break;
    case 7:
        ret = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(ff->ptr))(args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
        break;
    case 8:
        ret = ((uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))(ff->ptr))(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        break;
    default:
        // @CreateProcessA you fool
        THROW_FFI_ERROR("too many arguments, update ffi_callback to support it");
        goto cleanup;
    }

    switch (ff->return_type) {
    case TYPE_INVALID:
        LOG_ERROR("wha... how... idk, ur on ur own))");
        THROW_FFI_ERROR("you fool! i have been trained in your ffi arts by count dooku!");
        goto cleanup;
    case TYPE_VOID:
        goto cleanup;
    case TYPE_POINTER: {
        v8::Local<v8::BigInt> ret_bi(v8::BigInt::NewFromUnsigned(iso, (uint64_t)ret));
        info.GetReturnValue().Set(ret_bi);
        goto cleanup;
    }
    case TYPE_STRING: {
        if (!ret) {
            info.GetReturnValue().SetNull();
            goto cleanup;
        }

        v8::Local<v8::Value> ret_str;
        TO_LOCAL(ret_str, v8::String::NewFromUtf8(iso, (const char*)ret), "failed to parse return string to v8");
        info.GetReturnValue().Set(ret_str);
        goto cleanup;
    }
    default:
        info.GetReturnValue().Set(ret);
        goto cleanup;
    }

    // cleanup string allocs
cleanup:
    for (auto it = allocd_strs.begin(); it != allocd_strs.end(); it++) {
        delete* it;
    }
}

JS_HANDLER(JsNatives::ffi::resolve) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() < 3 || info.Length() > 4) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.ffi.resolve(): wrong number of arguments");
        return;
    }

    std::string library, symbol;
    ARG_STR(info[0], library);
    ARG_STR(info[1], symbol);

    uint32_t ret_type_num;
    ARG_UINT32(info[2], ret_type_num);
    FfiType ret_type = Utils::Ffi::type_from_int(ret_type_num);

    std::vector<FfiType>* arg_types = nullptr;

    if (info.Length() == 4) {
        arg_types = get_ffi_types(iso, ctx, info[3]);
        if (!arg_types) {
            return;
        }
    }

    jshandle_t h = STATE->resolve_function(library, symbol, ret_type, arg_types);
    delete arg_types;
    if (h == INVALID_HANDLE) {
        THROW_ERROR("couldn't resolve function %s in library %s", library.c_str(), symbol.c_str());
        return;
    }

    auto d = v8::Uint32::New(iso, h);
    auto f = v8::FunctionTemplate::New(iso, ffi_callback, d);

    info.GetReturnValue().Set(f->GetFunction(ctx).ToLocalChecked());
}

JS_HANDLER(JsNatives::ffi::define) {
    auto iso = info.GetIsolate();
    auto ctx = iso->GetCurrentContext();

    if (info.Length() < 2 || info.Length() > 3) {
        THROW_EXC(v8::Exception::TypeError, "invalid arguments to ctx.ffi.define(): wrong number of arguments");
        return;
    }

    uint64_t ptr;
    ARG_UINT64(info[0], ptr);

    uint32_t ret_type_num;
    ARG_UINT32(info[1], ret_type_num);
    FfiType ret_type = Utils::Ffi::type_from_int(ret_type_num);

    std::vector<FfiType>* arg_types = get_ffi_types(iso, ctx, info[2]);
    if (!arg_types) {
        return;
    }

    jshandle_t h = STATE->define_function((void*)ptr, ret_type, arg_types);
    delete arg_types;
    if (h == INVALID_HANDLE) {
        THROW_ERROR("couldn't define function");
        return;
    }

    auto d = v8::Uint32::New(iso, h);
    auto f = v8::FunctionTemplate::New(iso, ffi_callback, d);

    info.GetReturnValue().Set(f->GetFunction(ctx).ToLocalChecked());
}