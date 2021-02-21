#include "rpc_client.h"

#include "app.h"
#include "utils/uv_callbacks.h"
#include <iostream>
#include <cassert>
#include <cstring>

namespace ryu {

Result<ResultVoid, std::string> RpcClient::Accept(uv_stream_t* server) {
    int retcode = 0;

    buf_capacity_ = 1024;
    buf_size_ = 0;
    data_buf_ = std::make_unique<char[]>(buf_capacity_);
    socket_ = std::make_unique<uv_tcp_t>();
    retcode = uv_tcp_init(server->loop, socket_.get());
    if (retcode) return Err("uv_tcp_init() failed");
    socket_->data = this;

    retcode = uv_accept(server, reinterpret_cast<uv_stream_t*>(socket_.get()));
    if (retcode) return Err("uv_accept() failed");
    retcode = uv_read_start(
        (uv_stream_t*) socket_.get(),
        uv_callbacks::Alloc<&RpcClient::BufferSelection>,
        uv_callbacks::Read<&RpcClient::IncomingData>
    );
    if (retcode) return Err("uv_read_start() failed");
    return {};
}

void RpcClient::Halt() {
    std::cout << "Halting RpcClient" << std::endl;
    if (socket_) {
        uv_read_stop((uv_stream_t*)socket_.get());
        uv_close((uv_handle_t*)socket_.get(), uv_callbacks::Close<&RpcClient::SocketClosed>);
    } else {
        app_->ReleaseRpcClient(*this);
    }
}

void RpcClient::IncomingCommand(std::string str) {
    std::cout << "Received RPC command: " << str << std::endl;
    if (str == "bye") {
        Halt();
    } else if (str == "stop") {
        app_->Halt();
    }
}

void RpcClient::BufferSelection(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    assert(buf_size_ < buf_capacity_);
    buf->base = data_buf_.get() + buf_size_;
    buf->len = buf_capacity_  - buf_size_;
}

void RpcClient::IncomingData(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if (nread == UV_EOF) {
        Halt();
    } else if (nread < 0) {
        Halt();
    } else if (nread == 0) {
        return;
    } 

    size_t st = 0;
    for (size_t idx = buf_size_; idx < buf_size_ + nread; idx++) {
        if (data_buf_[idx] == '\n') {
            size_t cnt = idx - st;
            IncomingCommand(std::string(data_buf_.get() + st, cnt));
            st = idx+1;
        }
    }
    if (st < buf_size_+nread) {
        size_t new_size = buf_size_ + nread - st;
        memmove(data_buf_.get(), data_buf_.get() + st, new_size);
        buf_size_ = new_size;
    }
}

void RpcClient::SocketClosed(uv_handle_t* handle) {
    assert(handle == (uv_handle_t*)socket_.get());
    app_->ReleaseRpcClient(*this);
}

}  // namespace ryu