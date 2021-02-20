#include "app.h"

#include <memory>
#include "result.h"
#include <cassert>
#include <iostream>

namespace ryu {

Result<int, std::string> App::Run() {
    rpc_manager_ = std::make_unique<RpcManager>(this);
    VALUE_OR_RAISE(rpc_manager_->Listen("[::1]:8989", loop_));
    std::cout << "Listening on [::1]:8989" << std::endl;
    return uv_run(loop_, UV_RUN_DEFAULT);
}

void App::AcceptRpcClient(uv_stream_t* server) {
    auto client = std::make_shared<RpcClient>(this);
    client->Accept(server).Expect("Failed to accept client");
    rpc_clients_[client.get()] = client;
}

void App::Halt() {
    // TODO
}

void App::RpcManagerClosed(RpcManager& rpc_manager) {
    assert(&rpc_manager == rpc_manager_.get());
    rpc_manager_.reset();
}

}