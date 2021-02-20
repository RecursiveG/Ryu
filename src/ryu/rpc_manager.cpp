#include "rpc_manager.h"
#include "app.h"
#include "result.h"
#include "common/network.h"
#include <uv.h>
#include "utils/uv_callbacks.h"
#include <cassert>
#include <iostream>

namespace ryu {

Result<ResultVoid, std::string> RpcManager::Listen(std::string listen_addr, uv_loop_t* loop) {
    int retcode = 0;
    sockaddr_storage addr = {};

    VALUE_OR_RAISE(net::ParseAddrPort(listen_addr, &addr, 8989));

    socket_ = std::make_unique<uv_tcp_t>();
    retcode = uv_tcp_init(loop, socket_.get());
    if (retcode) return Err("RpcManager::Listen uv_tcp_init() failed");
    socket_->data = this;

    retcode = uv_tcp_bind(socket_.get(), reinterpret_cast<sockaddr*>(&addr), UV_TCP_IPV6ONLY);
    if (retcode) return Err("uv_tcp_bind() failed");
    retcode = uv_listen(reinterpret_cast<uv_stream_t*>(socket_.get()), 512, 
        uv_callbacks::Connection<&RpcManager::NewConnection>);
    if (retcode) return Err("uv_listen() failed");
    return {};
}

void RpcManager::Halt() {
    // TODO
}

void RpcManager::NewConnection(uv_stream_t* server, int status) {
    assert(server == (uv_stream_t*)socket_.get());
    assert(status == 0);
    std::cout << "Received incoming connection" << std::endl;
    // TODO
}

}