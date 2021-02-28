#include "rpc_client.h"

#include "app.h"
#include "utils/uv_callbacks.h"
#include <iostream>
#include <cassert>
#include <cstring>
#include "absl/strings/match.h"

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
    // finishes all write
    // shutdown write end
    // read until eof
    // close socket
    // notify app
    std::cout << "Halting RpcClient" << std::endl;
    draining_ = true;

    shutdown_req_.data = this;
    uv_shutdown(&shutdown_req_, (uv_stream_t*)socket_.get(), uv_callbacks::Shutdown<&RpcClient::SocketShutdownComplete>);
}

void RpcClient::SocketShutdownComplete(uv_shutdown_t* req, int status) {
    assert(status == 0);
    assert(req = &shutdown_req_);
    socket_shutdown_ = true;
    CheckSocketReadyToClose();
}

void RpcClient::CheckSocketReadyToClose() {
    if (!draining_) {
        // socket shutdown haven't been issued, halting caused by eof.
        Halt();
        return;
    }
    if (!socket_eof_ || !socket_shutdown_) return;
    uv_close((uv_handle_t*)socket_.get(), uv_callbacks::Close<&RpcClient::SocketClosed>);
}

void RpcClient::SocketClosed(uv_handle_t* handle) {
    assert(handle == (uv_handle_t*)socket_.get());
    app_->ReleaseRpcClient(*this);
}

void RpcClient::IncomingCommand(std::string str) {
    std::cout << "Received RPC command: " << str << std::endl;
    if (str == "bye") {
        Halt();
    } else if (str == "stop") {
        app_->Halt();
    } else if (str == "ping") {
        auto buf = std::make_unique<UvWriteBuf>(5);
        buf->buffer()[0] = 'p';
        buf->buffer()[1] = 'o';
        buf->buffer()[2] = 'n';
        buf->buffer()[3] = 'g';
        buf->buffer()[4] = '\n';
        buf->write(5, (uv_stream_t*)socket_.get(), this, uv_callbacks::Write<&RpcClient::WriteFinishes>);
        outgoing_[buf.get()] = std::move(buf);
    } else if (absl::StartsWithIgnoreCase(str, "CreateTask ")) {
        app_->CreateTask(str.substr(11));
    }
}

void RpcClient::BufferSelection(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    assert(buf_size_ < buf_capacity_);
    buf->base = data_buf_.get() + buf_size_;
    buf->len = buf_capacity_  - buf_size_;
}

void RpcClient::IncomingData(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if (nread == UV_EOF) {
        socket_eof_ = true;
        CheckSocketReadyToClose();
        return;
    } else if (nread < 0) {
        Halt();
        return;
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

void RpcClient::WriteFinishes(uv_write_t* write_req, int status) {
    auto iter = outgoing_.find((UvWriteBuf*)write_req);
    assert(iter != outgoing_.end());
    assert(status == 0);
    outgoing_.erase(iter);
}

}  // namespace ryu