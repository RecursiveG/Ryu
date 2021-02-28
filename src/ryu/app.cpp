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
    draining_ = true;
    if (rpc_manager_) rpc_manager_->Halt();
    for (auto& [client, ptr] : rpc_clients_) {
        client->Halt();
    }
}

void App::CheckDrainState() {
    if (!draining_) return;
    if (rpc_manager_) return;
    if (!rpc_clients_.empty()) return;
    uv_stop(loop_);
}

void App::CreateTask(std::string torrent_file_name) {
    auto task = std::make_shared<Task>(this, loop_, torrent_file_name);
    tasks_[task.get()] = task;
}

void App::ReleaseRpcManager(RpcManager& rpc_manager) {
    assert(&rpc_manager == rpc_manager_.get());
    rpc_manager_.reset();
    CheckDrainState();
}

void App::ReleaseRpcClient(RpcClient& rpc_client) {
    auto iter = rpc_clients_.find(&rpc_client);
    assert(iter != rpc_clients_.end());
    rpc_clients_.erase(iter);
    std::cout << "RPC client released, remaining: " << rpc_clients_.size() << std::endl;
    CheckDrainState();
}

}