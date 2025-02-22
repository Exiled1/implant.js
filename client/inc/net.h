#pragma once

#include <csignal>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <memory>

#ifdef LINUX
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#endif

#include "log.h"
#include "module.h"
#include "utils.h"
#include "winhdr.h"

#ifdef LINUX
typedef int socket_t;
#endif
#ifdef WINDOWS
typedef SOCKET socket_t;
#endif

struct CallFrame {
    int lineno;
    std::string symbol;
};

class DebugRespPacket {
public:
    enum {
        READY = 0xf0,
        CONTEXT = 0xf1,
        OUTPUT = 0xf2,
        BREAKSET = 0xf3,
        EVAL = 0xf4
    } type;

    // set iff type==CONTEXT
    struct {
        std::vector<CallFrame> frames;
        uint8_t status;
        std::string exc;
    } ctx_data;

    // set iff type==OUTPUT
    struct {
        std::string output;
    } output_data;

    // set iff type==BREAKSET
    struct {
        bool success;
        uint32_t lineno;
        std::string id;
    } breakset_data;

    // set iff type==EVAL
    struct {
        std::string output;
        bool error;
    } eval_data;

    ~DebugRespPacket() {}

    static std::shared_ptr<DebugRespPacket> Ready();
    static std::shared_ptr<DebugRespPacket> Context(std::vector<CallFrame> frames, uint8_t status = STATUS_RUNNING, std::string exc = "");
    static std::shared_ptr<DebugRespPacket> Output(std::string output);
    static std::shared_ptr<DebugRespPacket> BreakSet(bool success, uint32_t lineno, std::string id);
    static std::shared_ptr<DebugRespPacket> Eval(std::string output, bool error);

    std::vector<uint8_t> pack();

private:
    DebugRespPacket(decltype(DebugRespPacket::type) type);
};

class DebugCmdPacket {
public:
    enum {
        CONTINUE = 0xe0,
        QUIT = 0xe1,
        STEP = 0xe2,
        NEXT = 0xe3,
        STEPOUT = 0xe4,
        BREAKSET = 0xe5,
        BREAKCLEAR = 0xe6,
        EVAL = 0xe7
    } type;

    // set iff type==BREAKSET
    struct {
        uint32_t lineno;
    } breakset_data;

    // set iff type==BREAKCLEAR
    struct {
        std::string id;
    } breakclear_data;

    // set iff type==EVAL
    struct {
        std::string expr;
    } eval_data;

    ~DebugCmdPacket() {}

    static std::shared_ptr<DebugCmdPacket> Continue();
    static std::shared_ptr<DebugCmdPacket> Quit();
    static std::shared_ptr<DebugCmdPacket> Step();
    static std::shared_ptr<DebugCmdPacket> Next();
    static std::shared_ptr<DebugCmdPacket> StepOut();
    static std::shared_ptr<DebugCmdPacket> BreakSet(uint32_t lineno);
    static std::shared_ptr<DebugCmdPacket> BreakClear(std::string id);
    static std::shared_ptr<DebugCmdPacket> Eval(std::string expr);

    static std::shared_ptr<DebugCmdPacket> fetch(socket_t socket);

private:
    DebugCmdPacket(decltype(DebugCmdPacket::type) type);
};

class Net {
private:
    std::string hostname;
    uint16_t port;

    socket_t socket;
    bool connected;
public:
    Net(std::string, uint16_t);
    ~Net();

    bool Connect();

    Module* FetchModule();
    bool SendResponse(uint8_t status, std::string output);

    bool SendDebugResp(std::shared_ptr<DebugRespPacket> pkt);
    std::shared_ptr<DebugCmdPacket> RecvDebugCmd();
};