#include "net.h"

#define HS_SYN "\x13\x37"
#define HS_ACK "\x73\x31"

#define PKT_FETCH "\x80"
#define PKT_MODULE "\x81"
#define PKT_RESP "\x82"
#define PKT_NOOP "\x90"
#define PKT_BYE "\x91"
#define PKT_DBG "\xdd"

#define PACK_UINT8(val) { \
    output.push_back((uint8_t)(val)); \
}

#define PACK_UINT32(val) { \
    output.resize(output.size() + 4); \
    uint32_t* ptr = (uint32_t*)(output.data() + output.size() - 4); \
    auto v = htonl((val)); \
    *ptr = v; \
}

#define PACK_STRING(val) { \
    PACK_UINT32((val).length()); \
    output.insert(output.end(), (val).begin(), (val).end()); \
}

#define PACK_BOOL(val) { \
    output.push_back((val)); \
}

#define UNPACK_UINT32(val) { \
    if (recv(socket, (char *)&(val), 4, 0) == -1) { \
        LOG_ERROR_NET("failed to recv " #val " from server"); \
        return nullptr; \
    } \
    (val) = ntohl((val)); \
}

#define UNPACK_STR(val) { \
    uint32_t str_sz; \
    UNPACK_UINT32(str_sz) \
    (val).resize(str_sz); \
    if (recv(socket, (val).data(), str_sz, 0) == -1) { \
        LOG_ERROR_NET("failed to recv " #val " from server"); \
        return nullptr; \
    } \
}

DebugRespPacket::DebugRespPacket(decltype(DebugRespPacket::type) type) : type(type) {
    if (this->type == DebugRespPacket::OUTPUT) {
        this->output_data.output = std::string();
    }
}

std::shared_ptr<DebugRespPacket> DebugRespPacket::Ready() {
    return std::shared_ptr<DebugRespPacket>(new DebugRespPacket(DebugRespPacket::READY));
}

std::shared_ptr<DebugRespPacket> DebugRespPacket::Context(std::vector<CallFrame> frames, uint8_t status, std::string exc) {
    auto p = std::shared_ptr<DebugRespPacket>(new DebugRespPacket(DebugRespPacket::CONTEXT));
    p->ctx_data.frames = frames;
    p->ctx_data.status = status;
    p->ctx_data.exc = exc;
    return p;
}

std::shared_ptr<DebugRespPacket> DebugRespPacket::Output(std::string output) {
    auto p = std::shared_ptr<DebugRespPacket>(new DebugRespPacket(DebugRespPacket::OUTPUT));
    p->output_data.output = output;
    return p;
}

std::shared_ptr<DebugRespPacket> DebugRespPacket::BreakSet(bool success, uint32_t lineno, std::string id) {
    auto p = std::shared_ptr<DebugRespPacket>(new DebugRespPacket(DebugRespPacket::BREAKSET));
    p->breakset_data.success = success;
    p->breakset_data.lineno = lineno;
    p->breakset_data.id = id;
    return p;
}

std::shared_ptr<DebugRespPacket> DebugRespPacket::Eval(std::string output, bool error) {
    auto p = std::shared_ptr<DebugRespPacket>(new DebugRespPacket(DebugRespPacket::EVAL));
    p->eval_data.output = output;
    p->eval_data.error = error;
    return p;
}

std::vector<uint8_t> DebugRespPacket::pack() {
    auto output = std::vector<uint8_t>();
    PACK_UINT8(this->type);

    if (this->type == DebugRespPacket::CONTEXT) {
        PACK_UINT8(this->ctx_data.status);
        PACK_UINT32(this->ctx_data.frames.size());

        for (auto it = this->ctx_data.frames.begin(); it != this->ctx_data.frames.end(); it++) {
            auto f = *it;

            PACK_UINT32(f.lineno);
            PACK_STRING(f.symbol);
        }

        PACK_STRING(this->ctx_data.exc);
    }
    else if (this->type == DebugRespPacket::OUTPUT) {
        PACK_STRING(this->output_data.output);
    }
    else if (this->type == DebugRespPacket::BREAKSET) {
        PACK_BOOL(this->breakset_data.success);
        PACK_UINT32(this->breakset_data.lineno);
        PACK_STRING(this->breakset_data.id);
    }
    else if (this->type == DebugRespPacket::EVAL) {
        PACK_STRING(this->eval_data.output);
        PACK_BOOL(this->eval_data.error);
    }

    return output;
}

//////////////////////////////////////////////////////////////////

DebugCmdPacket::DebugCmdPacket(decltype(DebugCmdPacket::type) type) : type(type) {
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::Continue() {
    return std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::CONTINUE));
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::Quit() {
    return std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::QUIT));
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::Step() {
    return std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::STEP));
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::Next() {
    return std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::NEXT));
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::StepOut() {
    return std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::STEPOUT));
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::BreakSet(uint32_t lineno) {
    auto p = std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::BREAKSET));
    p->breakset_data.lineno = lineno;
    return p;
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::BreakClear(std::string id) {
    auto p = std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::BREAKCLEAR));
    p->breakclear_data.id = id;
    return p;
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::Eval(std::string expr) {
    auto p = std::shared_ptr<DebugCmdPacket>(new DebugCmdPacket(DebugCmdPacket::EVAL));
    p->eval_data.expr = expr;
    return p;
}

std::shared_ptr<DebugCmdPacket> DebugCmdPacket::fetch(socket_t socket) {
    uint8_t type;
    if (recv(socket, (char *)&type, 1, 0) == -1) {
        LOG_ERROR_NET("failed to recv DBG[type] from server");
        return nullptr;
    }

    switch (type) {
    case DebugCmdPacket::CONTINUE: {
        return DebugCmdPacket::Continue();
    }
    case DebugCmdPacket::QUIT: {
        return DebugCmdPacket::Quit();
    }
    case DebugCmdPacket::STEP: {
        return DebugCmdPacket::Step();
    }
    case DebugCmdPacket::NEXT: {
        return DebugCmdPacket::Next();
    }
    case DebugCmdPacket::STEPOUT: {
        return DebugCmdPacket::StepOut();
    }
    case DebugCmdPacket::BREAKSET: {
        uint32_t lineno;
        UNPACK_UINT32(lineno);

        return DebugCmdPacket::BreakSet(lineno);
    }
    case DebugCmdPacket::BREAKCLEAR: {
        std::vector<char> strdata;
        UNPACK_STR(strdata);

        return DebugCmdPacket::BreakClear(std::string(strdata.begin(), strdata.end()));
    }
    case DebugCmdPacket::EVAL: {
        std::vector<char> strdata;
        UNPACK_STR(strdata);

        return DebugCmdPacket::Eval(std::string(strdata.begin(), strdata.end()));
    }
    default: {
        LOG_ERROR("unhandled debug packet type: %p", type);
        return nullptr;
    }
    }
}

//////////////////////////////////////////////////////////////////

Net::Net(std::string hostname, uint16_t port) {
    this->hostname = hostname;
    this->port = port;

    this->connected = false;

#ifdef LINUX
    signal(SIGPIPE, SIG_IGN);
#endif
}

Net::~Net() {
    if (this->connected) {
#ifdef LINUX
        close(this->socket);
#endif
#ifdef WINDOWS
        closesocket(this->socket);
#endif
        this->connected = false;
    }

#ifdef WINDOWS
    WSACleanup();
#endif
}

bool Net::Connect() {
    if (this->connected) {
        LOG_ERROR("net client already connected, can't make a new connection");
        return false;
    }

#ifdef WINDOWS
    WSADATA wsadata;
    int ret;
    if ((ret = WSAStartup(MAKEWORD(2, 2), &wsadata)) != 0) {
        LOG_ERROR("failed to initialize winsock: %d", ret);
        return false;
    }
#endif

    struct addrinfo* ai, * cur, hints = { 0 };

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(this->hostname.c_str(), std::to_string(this->port).c_str(), &hints, &ai) != 0) {
        LOG_ERROR_ERRNO("gai call failed");
        return false;
    }

    this->socket = -1;
    for (cur = ai; cur != nullptr; cur = cur->ai_next) {
        if ((this->socket = ::socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol)) == -1) continue;

        if (connect(this->socket, cur->ai_addr, cur->ai_addrlen) == 0) break;

#ifdef LINUX
        close(this->socket);
#endif
#ifdef WINDOWS
        closesocket(this->socket);
#endif
        this->socket = -1;
    }

    freeaddrinfo(ai);

    if (this->socket == -1) {
        LOG_ERROR("failed to initialize socket, no valid addrinfo identified");
        return false;
    }

    // handshake with the server
    char os = 0xcc;
#ifdef LINUX
    os = CONST_OS_LINUX;
#endif
#ifdef WINDOWS
    os = CONST_OS_WINDOWS;
#endif
    if (send(this->socket, HS_SYN, 2, 0) == -1) {
        LOG_ERROR_NET("failed to send handshake syn");
        return false;
    }
    if (send(this->socket, &os, 1, 0) == -1) {
        LOG_ERROR_NET("failed to send OS byte");
        return false;
    }

    char ack[2];
    if (recv(this->socket, ack, 2, 0) == -1) {
        LOG_ERROR_NET("failed to recv handshake ack");
        return false;
    }

    if (memcmp(HS_ACK, ack, 2) != 0) {
        LOG_ERROR("handshake with server failed");
        return false;
    }

    this->connected = true;
    return true;
}

Module* Net::FetchModule() {
    if (!this->connected) {
        LOG_ERROR("not connected to the server, can't fetch a module");
        throw std::runtime_error(nullptr);
    }

    if (send(this->socket, PKT_FETCH, 1, 0) == -1) {
        LOG_ERROR_NET("failed to send PKT_FETCH, server probably died");
        throw std::runtime_error(nullptr);
    }

    char pkt;
    if (recv(this->socket, &pkt, 1, 0) == -1) {
        LOG_ERROR_NET("failed to recv pkt from server");
        throw std::runtime_error(nullptr);
    }

    if (memcmp(&pkt, PKT_NOOP, 1) == 0) {
        return nullptr;
    }

    if (memcmp(&pkt, PKT_BYE, 1) == 0) {
        throw std::runtime_error("bye");
    }

    if (memcmp(&pkt, PKT_MODULE, 1) == 0) {
        auto mod = new Module();

        if (recv(this->socket, (char*)&mod->debug, 1, 0) == -1) {
            LOG_ERROR_NET("failed to get module debug data");
            throw std::runtime_error(nullptr);
        }

        uint32_t code_length;
        if (recv(this->socket, (char*)&code_length, 4, 0) == -1) {
            LOG_ERROR_NET("failed to get module code size");
            throw std::runtime_error(nullptr);
        }
        code_length = ntohl(code_length);

        char* buf = (char*)Utils::Mem::alloc_heap(code_length + 1);
        if (!buf) {
            LOG_ERROR_NET("failed to allocate memory to receive code");
            throw std::runtime_error(nullptr);
        }
        if (recv(this->socket, buf, code_length, 0) == -1) {
            LOG_ERROR_NET("failed to get module code");
            throw std::runtime_error(nullptr);
        }
        buf[code_length] = '\x00';

        mod->code = std::string(buf);
        Utils::Mem::free_heap(buf);
        return mod;
    }

    return nullptr;
}

bool Net::SendResponse(uint8_t status, std::string output) {
    if (!this->connected) {
        LOG_ERROR("not connected to the server, can't send a response");
        return false;
    }

    if (send(this->socket, PKT_RESP, 1, 0) == -1) {
        LOG_ERROR_NET("failed to send PKT_RESP header");
        return false;
    }

    if (send(this->socket, (char*)&status, 1, 0) == -1) {
        LOG_ERROR_NET("failed to send resp status");
        return false;
    }

    int len = htonl(output.length());
    if (send(this->socket, (char*)&len, 4, 0) == -1) {
        LOG_ERROR_NET("failed to send resp data length");
        return false;
    }

    if (send(this->socket, output.c_str(), output.length(), 0) == -1) {
        LOG_ERROR_NET("failed to send resp data");
        return false;
    }

    return true;
}

bool Net::SendDebugResp(std::shared_ptr<DebugRespPacket> pkt) {
    if (!this->connected) {
        LOG_ERROR("not connected to the server, can't send a debug packet");
        return false;
    }

    if (send(this->socket, PKT_DBG, 1, 0) == -1) {
        LOG_ERROR_NET("failed to send PKT_DBG header");
        return false;
    }


    auto data = pkt->pack();
    if (send(this->socket, (char *)data.data(), data.size(), 0) == -1) {
        LOG_ERROR_NET("failed to send PKT_DBG data");
        return false;
    }

    return true;
}

std::shared_ptr<DebugCmdPacket> Net::RecvDebugCmd() {
    if (!this->connected) {
        LOG_ERROR("not connected to the server, can't receive a debug packet");
        return nullptr;
    }

    char hdr;
    if (recv(this->socket, &hdr, 1, 0) == -1) {
        LOG_ERROR_NET("failed to recv hdr from server");
        return nullptr;
    }

    if (memcmp(&hdr, PKT_DBG, 1) != 0) {
        LOG_ERROR("didn't get a debug packet, got something else: %hhx", hdr);
        return nullptr;
    }

    auto p = DebugCmdPacket::fetch(this->socket);
    if (!p) {
        LOG_ERROR("failed to receive a debug packet");
        return nullptr;
    }

    return p;
}