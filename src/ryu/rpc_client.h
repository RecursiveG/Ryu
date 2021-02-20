#pragma once

#include <uv.h>

#include "result.h"

namespace ryu {

class App;
class RpcClient {
  public:
    explicit RpcClient(App* app) : app_(app) {}
    Result<ResultVoid, std::string> Accept(uv_stream_t* server);
    void Halt();

    // called when a command arrives
    void IncomingCommand(std::string str);

    // UV callback when data income
    void BufferSelection(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    void IncomingData(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
  private:
    App* const app_;

    // stores incoming data
    size_t buf_capacity_;
    size_t buf_size_;
    std::unique_ptr<char[]> data_buf_;
    
    std::unique_ptr<uv_tcp_t> socket_;
};

}  // namespace ryu