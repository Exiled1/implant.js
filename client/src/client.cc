#include <cstdint>
#include <chrono>
#include <iostream>
#include <thread>

#include "jseng.h"
#include "log.h"
#include "module.h"
#include "net.h"
#include "consts.h"

#define SLEEP_DURATION 1

int main(int argc, char* argv[]) {
    if (argc != 3) {
        LOG_ERROR("usage: %s [server host] [server port]", argv[0]);
        return 1;
    }

    std::string hostname = std::string(argv[1]);
    uint16_t port = std::stoi(argv[2]);

    Net net = Net(hostname, port);

    if (net.Connect()) {
        LOG_INFO("successfully connected to %s:%d", hostname.c_str(), port);
    }
    else {
        LOG_ERROR("failed to connect to %s:%d", hostname.c_str(), port);
        return 1;
    }

    State::Initialize(&net);
    JsEng eng = JsEng();
    while (true) {
        try {
            auto module = net.FetchModule();
            if (!module) {
                goto sleep;
            }

            bool ret = eng.RunModule(module);

            if (!module->debug) {
                if (!ret) {
                    net.SendResponse(STATUS_FAILURE, "execution failed with unrecoverable error");
                }
                else {
                    net.SendResponse(STATE->get_errord() ? STATUS_FAILURE : STATUS_SUCCESS, STATE->get_output());
                }
            }

            // reset the state for the next execution     
            State::Initialize(&net);

            delete module;
        }
        catch (const std::exception& e) {
            // on server->client termination, a "bye" runtime exception is raised
            // gracefully exit if this happened
            if (strncmp(e.what(), "bye", 3) == 0) {
                delete STATE;
                LOG_INFO("server disconnected, exiting");
                return 0;
            }

            delete STATE;
            return 1;
        }

    sleep:
        std::this_thread::sleep_for(std::chrono::seconds(SLEEP_DURATION));
    }
}