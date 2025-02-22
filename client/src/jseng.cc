#include "jseng.h"

#define CONST_GETTER(sym) v8::String::NewFromUtf8Literal(this->isolate, #sym), v8::FunctionTemplate::New(this->isolate, JsNatives::consts::const_##sym), v8::Local<v8::FunctionTemplate>(), v8::PropertyAttribute::ReadOnly

#define TO_LOCAL(local, maybe, err)                                        \
    if (!maybe.ToLocal(&local))                                            \
    {                                                                      \
        auto exc(try_catch.Exception());                                   \
        LOG_ERROR(err ": %s", *v8::String::Utf8Value(this->isolate, exc)); \
        return false;                                                      \
    }

#define V8_STR(str) v8::String::NewFromUtf8Literal(this->isolate, str)

JsEng::JsEng() {
    v8::V8::InitializeICUDefaultLocation("");

    this->platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(this->platform.get());
    v8::V8::Initialize();

    this->create_params = new v8::Isolate::CreateParams();
    this->create_params->array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
}

JsEng::~JsEng() {
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    delete this->create_params->array_buffer_allocator;
    delete this->create_params;
}

v8::Global<v8::Context> JsEng::buildContext() {
    auto mem = v8::ObjectTemplate::New(this->isolate);
    mem->Set(this->isolate, "alloc", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::alloc));
    mem->Set(this->isolate, "free", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::free));
    mem->Set(this->isolate, "read", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::read));
    mem->Set(this->isolate, "read_dword", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::read_dword));
    mem->Set(this->isolate, "read_qword", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::read_qword));
    mem->Set(this->isolate, "write", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::write));
    mem->Set(this->isolate, "write_dword", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::write_dword));
    mem->Set(this->isolate, "write_qword", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::write_qword));
    mem->Set(this->isolate, "copy", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::copy));
    mem->Set(this->isolate, "equal", v8::FunctionTemplate::New(this->isolate, JsNatives::mem::equal));

    auto fs = v8::ObjectTemplate::New(this->isolate);
    fs->Set(this->isolate, "open", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::open));
    fs->Set(this->isolate, "close", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::close));
    fs->Set(this->isolate, "read", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::read));
    fs->Set(this->isolate, "read_line", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::read_line));
    fs->Set(this->isolate, "read_all", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::read_all));
    fs->Set(this->isolate, "write", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::write));
    fs->Set(this->isolate, "seek", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::seek));
    fs->Set(this->isolate, "eof", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::eof));
    fs->Set(this->isolate, "delete_file", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::delete_file));
    fs->Set(this->isolate, "file_exists", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::file_exists));
    fs->Set(this->isolate, "dir_exists", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::dir_exists));
    fs->Set(this->isolate, "dir_contents", v8::FunctionTemplate::New(this->isolate, JsNatives::fs::dir_contents));

    auto ffi = v8::ObjectTemplate::New(this->isolate);
    ffi->Set(this->isolate, "resolve", v8::FunctionTemplate::New(this->isolate, JsNatives::ffi::resolve));
    ffi->Set(this->isolate, "define", v8::FunctionTemplate::New(this->isolate, JsNatives::ffi::define));

    auto ctx = v8::ObjectTemplate::New(this->isolate);
    ctx->Set(this->isolate, "output", v8::FunctionTemplate::New(this->isolate, JsNatives::output));
    ctx->Set(this->isolate, "system", v8::FunctionTemplate::New(this->isolate, JsNatives::system));
    ctx->Set(this->isolate, "os", v8::FunctionTemplate::New(this->isolate, JsNatives::os));
    ctx->Set(this->isolate, "mem", mem);
    ctx->Set(this->isolate, "fs", fs);
    ctx->Set(this->isolate, "ffi", ffi);

    auto globals = v8::ObjectTemplate::New(this->isolate);
    globals->Set(this->isolate, "ctx", ctx);
    globals->SetAccessorProperty(CONST_GETTER(MEM_RW));
    globals->SetAccessorProperty(CONST_GETTER(MEM_RWX));

    globals->SetAccessorProperty(CONST_GETTER(MODE_R));
    globals->SetAccessorProperty(CONST_GETTER(MODE_W));
    globals->SetAccessorProperty(CONST_GETTER(MODE_RW));

    globals->SetAccessorProperty(CONST_GETTER(SEEK_SET));
    globals->SetAccessorProperty(CONST_GETTER(SEEK_END));
    globals->SetAccessorProperty(CONST_GETTER(SEEK_CUR));

    globals->SetAccessorProperty(CONST_GETTER(TYPE_VOID));
    globals->SetAccessorProperty(CONST_GETTER(TYPE_INTEGER));
    globals->SetAccessorProperty(CONST_GETTER(TYPE_POINTER));
    globals->SetAccessorProperty(CONST_GETTER(TYPE_BOOL));
    globals->SetAccessorProperty(CONST_GETTER(TYPE_STRING));

    globals->SetAccessorProperty(CONST_GETTER(OS_LINUX));
    globals->SetAccessorProperty(CONST_GETTER(OS_WINDOWS));

    return v8::Global<v8::Context>(this->isolate, v8::Context::New(this->isolate, nullptr, globals));
}

static const std::regex RE_LINE("^    at .*\\(?" MODULE_NAME ":(\\d+):\\d+\\)?$");
std::string JsEng::enrich_stack_trace(v8::TryCatch* try_catch, std::string const& script) {
    auto ctx = this->isolate->GetCurrentContext();

    v8::Local<v8::Value> raw_stack_trace;
    if (!(try_catch->StackTrace(ctx).ToLocal(&raw_stack_trace)) && raw_stack_trace->IsString()) {
        LOG_ERROR("failed to load stack trace from v8");
        return nullptr;
    }

    std::string stack_trace = std::string(*v8::String::Utf8Value(this->isolate, raw_stack_trace));

    // example stack_trace:
    // Error: false is not strictly true
    //     at TEST_STRICTFALSE (module.js:51:15)
    //     at module.js:67:1

    auto stack_trace_lines = Utils::get_lines(stack_trace);
    auto code_lines = Utils::get_lines(script);

    unsigned int idx = 0;
    while (idx < stack_trace_lines.size()) {
        std::smatch m;
        if (!std::regex_search(stack_trace_lines[idx++], m, RE_LINE)) {
            continue;
        }

        int lineno = std::stoi(m[1].str());

        // remove preceeding whitespace
        auto line = code_lines[lineno - 1];
        line.erase(0, line.find_first_not_of(' '));

        // insert the code context indented into the stack trace
        stack_trace_lines.insert(stack_trace_lines.begin() + idx, "        " + line);
    }

    return Utils::merge_lines(stack_trace_lines, "\n");
}

bool JsEng::RunModule(Module* mod) {
    if (!mod) {
        LOG_ERROR("got a nullptr for mod, can't execute it");
        return false;
    }

    if (mod->debug && !NET) {
        LOG_WARN("can't run this module in debug mode, no net client available");
        mod->debug = false;
    }

    this->isolate = v8::Isolate::New(*this->create_params);

    {
        v8::Isolate::Scope isolate_scope(this->isolate);
        v8::HandleScope handle_scope(this->isolate);

        auto gContext = this->buildContext();
        auto context = gContext.Get(this->isolate);

        v8::Context::Scope context_scope(context);

        LOG_INFO("executing js module, %d bytes of code", mod->code.length());

        v8::Local<v8::String> source;
        {
            v8::TryCatch try_catch(this->isolate);
            TO_LOCAL(source, v8::String::NewFromUtf8(this->isolate, mod->code.c_str()), "failed to load JS code into v8 obj");
        }

        // TODO: send compile errors back to the server

        if (mod->debug) {
            LOG_INFO("running module in debug mode");
            DbgEng* dbgr = new DbgEng(this->platform.get(), context);
            dbgr->Execute(source);
            delete dbgr;
        }
        else {
            v8::TryCatch try_catch(this->isolate);
            auto origin = v8::ScriptOrigin(V8_STR(MODULE_NAME));
            v8::Local<v8::Script> script;
            TO_LOCAL(script, v8::Script::Compile(context, source, &origin), "failed to compile JS code");

            v8::Local<v8::Value> result;
            auto maybe_result = script->Run(context);
            if (!maybe_result.ToLocal(&result) || try_catch.HasCaught()) {
                LOG_ERROR("module threw an exception");
                auto stack = this->enrich_stack_trace(&try_catch, mod->code);

                STATE->add_output(stack);
                STATE->set_errord();
            }
        }

        if (STATE->get_output().length() > 0) {
            LOG_INFO("module finished, output:");
            std::cout << STATE->get_output() << std::endl;
        }
        else {
            LOG_INFO("module successfully executed with no output");
        }
    }

    this->isolate->Dispose();

    return true;
}
