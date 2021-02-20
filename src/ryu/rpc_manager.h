#pragma once

#include <uv.h>
#include "result.h"
#include <string>

namespace ryu {

class App;
class RpcManager {
    public:
        explicit RpcManager(App* app) : app_(app) { }
        // Bind to a port before uv loop starts.
        Result<ResultVoid, std::string> Listen(std::string listen_addr, uv_loop_t* loop);
        // Stop listening and call App::RpcManagerClosed
        void Halt();

        // UV callback when new incoming connection
        void NewConnection(uv_stream_t* server, int status);
    private:
        App* const app_;
        std::unique_ptr<uv_tcp_t> socket_;
};

}