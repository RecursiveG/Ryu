#pragma once

#include <uv.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "result.h"
#include "ryu/rpc_client.h"
#include "ryu/rpc_manager.h"

namespace ryu {

class App {
  public:
    explicit App(uv_loop_t* loop) : loop_(loop) {}
    // Start libuv event loop
    Result<int, std::string> Run();
    // Called by rpc manager
    void AcceptRpcClient(uv_stream_t* server);
    // Cleanup resources. Causes Run() to return.
    void Halt();
    // Check draining state and stop the loop if drained. Caused by Halt() and Release*() calls
    void CheckDrainState();

    // Call caused by RpcManager::Halt()
    void ReleaseRpcManager(RpcManager& rpc_manager);
    // Called by RpcClient::SocketClosed()/Halt()
    void ReleaseRpcClient(RpcClient& rpc_client);

  private:
    uv_loop_t* const loop_;
    std::unique_ptr<RpcManager> rpc_manager_;
    std::unordered_map<RpcClient*, std::shared_ptr<RpcClient>> rpc_clients_;
    bool draining_ = false;
};

}  // namespace ryu