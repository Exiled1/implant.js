#pragma once

#include <iostream>
#include <regex>

#include <libplatform/libplatform.h>
#include <v8.h>

#include "consts.h"
#include "jsdbg.h"
#include "jsnatives.h"
#include "log.h"
#include "module.h"
#include "state.h"

class JsEng {
private:
    std::unique_ptr<v8::Platform> platform;
    v8::Isolate::CreateParams* create_params;
    v8::Isolate* isolate;

    v8::Global<v8::Context> buildContext();
    std::string enrich_stack_trace(v8::TryCatch* try_catch, std::string const& script);

public:
    JsEng();
    ~JsEng();

    bool RunModule(Module*);
};