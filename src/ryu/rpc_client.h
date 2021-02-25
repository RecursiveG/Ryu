#pragma once

#include <uv.h>
#include "utils/uv_write_buf.h"

#include "result.h"

namespace ryu {

class App;
class RpcClient {
  public:
    explicit RpcClient(App* app) : app_(app) {}
    Result<ResultVoid, std::string> Accept(uv_stream_t* server);

    // Forcefully close both send and recv side
    void Halt();
    // UV callback for shutdown, also mean that all writes are finished
    void SocketShutdownComplete(uv_shutdown_t* req, int status);
    // called by IncomingData() or SocketShutdownComplete()
    void CheckSocketReadyToClose();
    // UV callback when ready to release resouces, caused by Halt()
    void SocketClosed(uv_handle_t* handle);

    // called when a command arrives
    void IncomingCommand(std::string str);

    // UV callback when data income
    void BufferSelection(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    void IncomingData(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    // UV callback on write finish.
    void WriteFinishes(uv_write_t* write_req, int status);

  private:
    App* const app_;

    // stores incoming data
    size_t buf_capacity_;
    size_t buf_size_;
    std::unique_ptr<char[]> data_buf_;

    std::unique_ptr<uv_tcp_t> socket_;
    std::unordered_map<UvWriteBuf*, std::unique_ptr<UvWriteBuf>> outgoing_;

    uv_shutdown_t shutdown_req_;
    bool draining_ = false;
    bool socket_eof_ = false;
    bool socket_shutdown_ = false;
};

}  // namespace ryu