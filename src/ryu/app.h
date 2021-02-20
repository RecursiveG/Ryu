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

    // Call caused by RpcManager::Halt()
    void RpcManagerClosed(RpcManager& rpc_manager);

  private:
    uv_loop_t* const loop_;
    std::unique_ptr<RpcManager> rpc_manager_;
    std::unordered_map<RpcClient*, std::shared_ptr<RpcClient>> rpc_clients_;
};

}  // namespace ryu