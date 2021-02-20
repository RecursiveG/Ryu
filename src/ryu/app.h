#pragma once

#include <uv.h>

#include <string>

#include "result.h"

#include "ryu/rpc_manager.h"

namespace ryu {

class App {
  public:
    explicit App(uv_loop_t* loop) : loop_(loop) {}
    // Start libuv event loop
    Result<int, std::string> Run();
    // Cleanup resources. Causes Run() to return.
    void Halt();

    // Call caused by RpcManager::Halt()
    void RpcManagerClosed(RpcManager& rpc_manager);

  private:
    uv_loop_t* const loop_;
    std::unique_ptr<RpcManager> rpc_manager_;
};

}  // namespace ryu