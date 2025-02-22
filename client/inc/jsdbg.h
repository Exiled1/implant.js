#pragma once

#include <v8.h>
#include <v8-inspector.h>
#include <libplatform/libplatform.h>

#include "consts.h"
#include "log.h"
#include "net.h"
#include "state.h"

class JsonObj {
private:
    v8::Local<v8::Context> _context;
    v8::Isolate* _isolate;
    v8::Local<v8::Object> _obj;

    JsonObj(v8::Local<v8::Context>, v8::Local<v8::Object>);
public:
    JsonObj(v8::Local<v8::Context>);
    JsonObj(v8::Local<v8::Context>, std::string);

    ~JsonObj();

    bool HasKey(std::string);

    int GetInt(std::string);
    std::string GetString(std::string);
    std::shared_ptr<JsonObj> GetObj(std::string);
    std::shared_ptr<std::vector<std::shared_ptr<JsonObj>>> GetObjArray(std::string);

    void SetInt(std::string, int);
    void SetString(std::string, std::string);
    void SetObj(std::string, const JsonObj&);

    std::string AsString();
};

class DbgChan : public v8_inspector::V8Inspector::Channel {
private:
    v8::Global<v8::Context> _context;
    v8::Isolate* _isolate;

    std::function<void(const std::string&)> onScriptId;

    std::string v8BufToString(std::unique_ptr<v8_inspector::StringBuffer>);

public:
    DbgChan(v8::Local<v8::Context>, std::function<void(const std::string&)>);
    ~DbgChan();

    void sendResponse(int callId, std::unique_ptr<v8_inspector::StringBuffer> message) override;
    void sendNotification(std::unique_ptr<v8_inspector::StringBuffer> message) override;
    void flushProtocolNotifications() override;
};

class DbgEng : v8_inspector::V8InspectorClient {
private:
    v8::Platform* _platform;
    v8::Global<v8::Context> _context;
    v8::Isolate* _isolate;
    DbgChan* _chan;
    std::unique_ptr<v8_inspector::V8Inspector> _inspector;
    std::unique_ptr<v8_inspector::V8InspectorSession> _session;

    // id of the script being executed
    std::string _script_id;

    // debugger ipc function call IDs
    uint32_t _next_msg_id;

    // set to false in quit if it needs to finish
    bool _running = true;

    // set to true if QUIT was sent to terminate execution early
    bool _termd = false;

    void CallMethod(std::string, const JsonObj* = nullptr);

public:
    DbgEng(v8::Platform*, v8::Local<v8::Context>);
    ~DbgEng();

    bool Execute(v8::Local<v8::String>);
    void runMessageLoopOnPause(int contextGroupId) override;
    void quitMessageLoopOnPause() override;
};