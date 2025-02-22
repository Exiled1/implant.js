#include "jsdbg.h"

#define TO_LOCAL(local, maybe, err)                                        \
    if (!maybe.ToLocal(&local))                                            \
    {                                                                      \
        auto exc(try_catch.Exception());                                   \
        LOG_ERROR(err ": %s", *v8::String::Utf8Value(this->_isolate, exc)); \
        return false;                                                      \
    }

#define V8_STR(str) v8::String::NewFromUtf8Literal(this->_isolate, str)

DbgChan::DbgChan(v8::Local<v8::Context> context, std::function<void(const std::string&)> onScriptId) {
    this->_isolate = context->GetIsolate();
    this->_context.Reset(this->_isolate, context);

    this->onScriptId = onScriptId;
}

DbgChan::~DbgChan() {}

std::string DbgChan::v8BufToString(std::unique_ptr<v8_inspector::StringBuffer> buf) {
    auto s = buf->string();

    std::string str;
    if (s.is8Bit()) {
        str = std::string((char*)s.characters8(), s.length());
    }
    else {
        auto l = s.length();
        auto p = s.characters16();
        str.reserve(l);
        for (unsigned int i = 0; i < l; i++) {
            str.append(std::string(1, (char)((p[i]) & 0xff)));  // this is trash
        }
    }

    return str;
}

void DbgChan::sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message) {
    auto str = this->v8BufToString(std::move(message));
    LOG_DEBUG("sendResponse - %s", str.c_str());

    auto context = this->_context.Get(this->_isolate);
    JsonObj obj(context, str);

    // in a perfect world, the call IDs would be tracked with some sync-safe data structure to 1:1 map up the calls and responses
    // however, i am lazy

    if (obj.HasKey("error")) {
        auto err = obj.GetObj("error");
        auto msg = err->GetString("message");

        // {"id":3,"error":{"code":-32000,"message":"Could not resolve breakpoint"}}
        if (msg == "Could not resolve breakpoint") {
            NET->SendDebugResp(DebugRespPacket::BreakSet(false, 0, ""));
        }
    }
    else if (obj.HasKey("result")) {
        auto res = obj.GetObj("result");

        if (res->HasKey("breakpointId")) {
            // {"id":3,"result":{"breakpointId":"4:1:0:3","actualLocation":{"scriptId":"3","lineNumber":1,"columnNumber":0}}}
            auto loc = res->GetObj("actualLocation");

            NET->SendDebugResp(DebugRespPacket::BreakSet(true, loc->GetInt("lineNumber"), res->GetString("breakpointId")));
        }
        else if (res->HasKey("result")) {
            // {"id":8,"result":{"result":{"type":"function","className":"Function","description":"function a() {\nctx.output(\"im a\");\n}","objectId":"1107162646270975959.1.16"}}}
            auto res2 = res->GetObj("result");
            auto desc = res2->GetString("description");
            auto err = res->HasKey("exceptionDetails");

            NET->SendDebugResp(DebugRespPacket::Eval(desc, err));
        }
    }
}

void DbgChan::sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) {
    auto str = this->v8BufToString(std::move(message));
    LOG_DEBUG("sendNotification - %s", str.c_str());

    auto context = this->_context.Get(this->_isolate);
    JsonObj obj(context, str);

    std::string method = obj.GetString("method");

    if (method == "Debugger.scriptParsed") {
        auto params = obj.GetObj("params");
        this->onScriptId(params->GetString("scriptId"));
    }
    else if (method == "Debugger.paused") {
        auto params = obj.GetObj("params");
        auto frames = params->GetObjArray("callFrames");

        std::vector<CallFrame> frameVec;
        for (auto it = frames->begin(); it != frames->end(); it++) {
            auto f = *it;

            frameVec.push_back(
                CallFrame{
                    .lineno = f->GetObj("location")->GetInt("lineNumber") + 1,  // v8 zero-indexes line numbers
                    .symbol = f->GetString("functionName")
                }
            );
        }

        auto p = DebugRespPacket::Context(frameVec);

        if (params->HasKey("reason") && params->GetString("reason") == "exception") {
            p->ctx_data.exc = params->GetObj("data")->GetString("description");
        }

        NET->SendDebugResp(p);
    }
}

void DbgChan::flushProtocolNotifications() {}

///////////////////////////////////////////////////////////

void output_cb(const std::string& msg) {
    NET->SendDebugResp(DebugRespPacket::Output(msg));
}

// https://github.com/mutable-org/mutable/blob/main/src/backend/V8Engine.cpp#L123C1-L125C2
inline v8_inspector::StringView make_string_view(const std::string& str) {
    return v8_inspector::StringView(reinterpret_cast<const uint8_t*>(str.data()), str.length());
}

DbgEng::DbgEng(v8::Platform* platform, v8::Local<v8::Context> context) {
    this->_platform = platform;
    this->_isolate = context->GetIsolate();
    this->_context.Reset(this->_isolate, context);

    STATE->set_output_callback(output_cb);

    this->_chan = new DbgChan(
        context,
        [this](std::string id) {
            this->_script_id = id;
            LOG_DEBUG("script id: %s", id.c_str());
        }
    );

    this->_inspector = v8_inspector::V8Inspector::create(this->_isolate, this);

    std::string state("mutable");
    this->_session = this->_inspector->connect(CONTEXT_GROUP_ID, this->_chan, make_string_view(state), v8_inspector::V8Inspector::kFullyTrusted, v8_inspector::V8Inspector::kWaitingForDebugger);

    this->_next_msg_id = 0;
}

DbgEng::~DbgEng() {
    this->_session->stop();
    STATE->set_output_callback(nullptr);
    delete this->_chan;
}

void DbgEng::CallMethod(std::string name, const JsonObj* params) {
    auto context = this->_context.Get(this->_isolate);
    JsonObj obj(context);
    obj.SetInt("id", this->_next_msg_id++);
    obj.SetString("method", name);
    if (params) {
        obj.SetObj("params", *params);
    }

    std::string body = obj.AsString();
    LOG_DEBUG("dispatching msg: %s", body.c_str());
    auto msg = make_string_view(body);
    this->_session->dispatchProtocolMessage(msg);
}

bool DbgEng::Execute(v8::Local<v8::String> source) {
    auto context = this->_context.Get(this->_isolate);
    std::string ctx_name("module");
    this->_inspector->contextCreated(v8_inspector::V8ContextInfo(context, CONTEXT_GROUP_ID, make_string_view(ctx_name)));

    this->CallMethod("Runtime.enable");
    this->CallMethod("Debugger.enable");
    JsonObj params(context);
    params.SetString("state", "uncaught");
    this->CallMethod("Debugger.setPauseOnExceptions", &params);

    auto origin = v8::ScriptOrigin(V8_STR(MODULE_NAME));
    v8::Local<v8::Script> script;
    {
        v8::TryCatch try_catch(this->_isolate);
        TO_LOCAL(script, v8::Script::Compile(context, source, &origin), "failed to compile JS code");
    }

    std::string reason("initial setup");
    auto reason_sv = make_string_view(reason);
    this->_session->schedulePauseOnNextStatement(reason_sv, reason_sv);

    NET->SendDebugResp(DebugRespPacket::Ready());

    v8::Local<v8::Value> result;
    auto maybe_result = script->Run(context);
    uint8_t status = this->_termd ? STATUS_TERM : STATUS_SUCCESS;
    if (!maybe_result.ToLocal(&result) && !this->_termd) {
        LOG_ERROR("module threw an exception");
        STATE->set_errord();
        status = STATUS_FAILURE;
    }

    NET->SendDebugResp(DebugRespPacket::Context(std::vector<CallFrame>(), status));

    this->_inspector->contextDestroyed(context);

    return true;
}

void DbgEng::runMessageLoopOnPause(int contextGroupId) {
    LOG_DEBUG("executing dbg msg loop");
    auto context = this->_context.Get(this->_isolate);
    this->_running = true;

    while (this->_running) {
        // LOG_DEBUG("outer loop iter");

        auto pkt = NET->RecvDebugCmd();
        if (!pkt) {
            LOG_ERROR("failed to receive a debug packet, can't run debug client");
            this->_running = false;
            break;
        }

        switch (pkt->type) {
        case DebugCmdPacket::CONTINUE: {
            LOG_INFO("resuming script execution");
            this->_session->resume();
            break;
        }
        case DebugCmdPacket::QUIT: {
            LOG_INFO("terminating script execution");
            this->_termd = true;
            this->_session->stop();
            this->CallMethod("Runtime.terminateExecution");
            break;
        }
        case DebugCmdPacket::STEP: {
            LOG_INFO("single stepping into");
            this->CallMethod("Debugger.stepInto");
            break;
        }
        case DebugCmdPacket::NEXT: {
            LOG_INFO("single stepping over");
            this->CallMethod("Debugger.stepOver");
            break;
        }
        case DebugCmdPacket::STEPOUT: {
            LOG_INFO("single stepping out");
            this->CallMethod("Debugger.stepOut");
            break;
        }
        case DebugCmdPacket::BREAKSET: {
            LOG_INFO("setting a breakpoint at line %d", pkt->breakset_data.lineno);

            JsonObj params(context), location(context);
            location.SetString("scriptId", this->_script_id);
            location.SetInt("lineNumber", pkt->breakset_data.lineno - 1);  // v8 zero-indexes the line numbers
            params.SetObj("location", location);

            this->CallMethod("Debugger.setBreakpoint", &params);
            break;
        }
        case DebugCmdPacket::BREAKCLEAR: {
            LOG_INFO("deleting breakpoint %s", pkt->breakclear_data.id.c_str());

            JsonObj params(context);
            params.SetString("breakpointId", pkt->breakclear_data.id);

            this->CallMethod("Debugger.removeBreakpoint", &params);
            break;
        }
        case DebugCmdPacket::EVAL: {
            LOG_INFO("evaling JS expression: %s", pkt->eval_data.expr.c_str());

            JsonObj params(context);
            params.SetString("expression", pkt->eval_data.expr);
            params.SetInt("contextId", CONTEXT_GROUP_ID);

            this->CallMethod("Runtime.evaluate", &params);
            break;
        }
        }

        while (v8::platform::PumpMessageLoop(this->_platform, this->_isolate)) {
            // LOG_DEBUG("inner loop iter");
        }
    }

    LOG_DEBUG("dbg msg loop finished");
}

void DbgEng::quitMessageLoopOnPause() {
    this->_running = false;
}

//////////////////////////////////////////////////////////

// vv helpful ref: https://github.com/ahmadov/v8_inspector_example/blob/master/src/utils.h

JsonObj::JsonObj(v8::Local<v8::Context> context) {
    this->_context = context;
    this->_isolate = context->GetIsolate();

    this->_obj = v8::Object::New(this->_isolate);
}

JsonObj::JsonObj(v8::Local<v8::Context> context, std::string msg) {
    this->_context = context;
    this->_isolate = context->GetIsolate();

    // based on https://github.com/ahmadov/v8_inspector_example/blob/master/src/utils.h#L25
    v8::MaybeLocal<v8::Value> value_ = v8::JSON::Parse(context, v8::String::NewFromUtf8(context->GetIsolate(), msg.data()).ToLocalChecked());
    if (value_.IsEmpty()) {
        this->_obj = v8::Object::New(this->_isolate);
    }
    else {
        this->_obj = value_.ToLocalChecked()->ToObject(context).ToLocalChecked();
    }
}

JsonObj::~JsonObj() {}

bool JsonObj::HasKey(std::string key) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    return this->_obj->Has(this->_context, key_).ToChecked();
}

int JsonObj::GetInt(std::string key) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    auto value = this->_obj->Get(this->_context, key_);

    v8::Local<v8::Value> local;
    if (!value.ToLocal(&local)) {
        LOG_ERROR("failed to get int val for %s from jsonobj", key.c_str());
        return 0;
    }

    return local->ToUint32(this->_context).ToLocalChecked()->Int32Value(this->_context).ToChecked();
}

std::string JsonObj::GetString(std::string key) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    auto value = this->_obj->Get(this->_context, key_);

    v8::Local<v8::Value> local;
    if (!value.ToLocal(&local)) {
        LOG_ERROR("failed to get str val for %s from jsonobj", key.c_str());
        return "";
    }

    v8::String::Utf8Value v8s(this->_isolate, local->ToString(this->_context).ToLocalChecked());
    return std::string(*v8s);
}

std::shared_ptr<JsonObj> JsonObj::GetObj(std::string key) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    auto value = this->_obj->Get(this->_context, key_);

    v8::Local<v8::Value> local;
    if (!value.ToLocal(&local)) {
        LOG_ERROR("failed to get obj val for %s from jsonobj", key.c_str());
        return nullptr;
    }

    return std::shared_ptr<JsonObj>(new JsonObj(this->_context, local->ToObject(this->_context).ToLocalChecked()));
}

std::shared_ptr<std::vector<std::shared_ptr<JsonObj>>> JsonObj::GetObjArray(std::string key) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    auto value = this->_obj->Get(this->_context, key_);

    v8::Local<v8::Value> local;
    if (!value.ToLocal(&local) || !local->IsArray()) {
        LOG_ERROR("failed to get array val for %s from jsonobj", key.c_str());
        return nullptr;
    }

    v8::Local<v8::Array> arr = local.As<v8::Array>();
    auto vec = std::shared_ptr<std::vector<std::shared_ptr<JsonObj>>>(new std::vector<std::shared_ptr<JsonObj>>());
    vec->reserve(arr->Length());
    for (unsigned int i = 0; i < arr->Length(); i++) {
        vec->push_back(std::shared_ptr<JsonObj>(new JsonObj(this->_context, arr->Get(this->_context, i).ToLocalChecked()->ToObject(this->_context).ToLocalChecked())));
    }

    return vec;
}

JsonObj::JsonObj(v8::Local<v8::Context> context, v8::Local<v8::Object> obj) {
    this->_context = context;
    this->_isolate = context->GetIsolate();
    this->_obj = obj;
}

void JsonObj::SetInt(std::string key, int value) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    auto value_ = v8::Int32::New(this->_isolate, value);

    this->_obj->Set(this->_context, key_, value_).ToChecked();
}

void JsonObj::SetString(std::string key, std::string value) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    auto value_ = v8::String::NewFromUtf8(this->_isolate, value.c_str()).ToLocalChecked();

    this->_obj->Set(this->_context, key_, value_).ToChecked();
}

void JsonObj::SetObj(std::string key, const JsonObj& value) {
    auto key_ = v8::String::NewFromUtf8(this->_isolate, key.c_str()).ToLocalChecked();
    this->_obj->Set(this->_context, key_, value._obj).ToChecked();
}

std::string JsonObj::AsString() {
    v8::Local<v8::String> out;
    auto maybe_str = v8::JSON::Stringify(this->_context, this->_obj);
    if (!maybe_str.ToLocal(&out)) {
        LOG_ERROR("failed to form json string");
        return "";
    }

    v8::String::Utf8Value v8s(this->_isolate, out);
    return std::string(*v8s);
}