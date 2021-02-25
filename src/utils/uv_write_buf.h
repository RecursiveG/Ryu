#pragma once

#include <uv.h>
#include <cstdint>
#include <memory>

namespace ryu {

class UvWriteBuf : public uv_write_t {
public:
    UvWriteBuf(size_t buf_capacity) {
        data_ = std::make_unique<char[]>(buf_capacity);
        buf_.base = data_.get();
    }
    char* buffer() { return data_.get(); }
    int write(size_t size, uv_stream_t* stream, void* callback_obj, uv_write_cb cb) {
        buf_.len = size;
        this->data = callback_obj;
        return uv_write((uv_write_t*)this, stream, &buf_, 1, cb);
    }
private:
    uv_buf_t buf_;
    std::unique_ptr<char[]> data_;
};

}