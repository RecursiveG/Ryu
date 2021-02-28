#pragma once

#include <uv.h>

#include <cstdint>
#include <string>

template <typename PtrT>
struct get_class_of_member_pointer : std::false_type {};
template <typename R, typename C>
struct get_class_of_member_pointer<R C::*> {
    using type = C;
};

namespace ryu::uv_callbacks {
#define USING_CLASS_TYPE                                                  \
    using MemberPtrType = decltype(member_ptr);                           \
    static_assert(std::is_member_function_pointer<MemberPtrType>::value); \
    using ClassType = typename get_class_of_member_pointer<MemberPtrType>::type;

template <auto member_ptr>
void Async(uv_async_t* handle) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(handle->data);
    (obj->*member_ptr)(handle);
}

template <auto member_ptr>
void Check(uv_check_t* handle) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(handle->data);
    (obj->*member_ptr)(handle);
}

template <auto member_ptr>
void Alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(handle->data);
    (obj->*member_ptr)(handle, suggested_size, buf);
}

template <auto member_ptr>
void Close(uv_handle_t* handle) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(handle->data);
    (obj->*member_ptr)(handle);
}

template <auto member_ptr>
void Read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(stream->data);
    (obj->*member_ptr)(stream, nread, buf);
}

template <auto member_ptr>
void Write(uv_write_t* req, int status) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(req->data);
    (obj->*member_ptr)(req, status);
}

template <auto member_ptr>
void Connect(uv_connect_t* req, int status) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(req->data);
    (obj->*member_ptr)(req, status);
}

template <auto member_ptr>
void Shutdown(uv_shutdown_t* req, int status) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(req->data);
    (obj->*member_ptr)(req, status);
}

template <auto member_ptr>
void Connection(uv_stream_t* server, int status) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(server->data);
    (obj->*member_ptr)(server, status);
}

template <auto member_ptr>
void Fs(uv_fs_t* req) {
    USING_CLASS_TYPE;
    ClassType* obj = static_cast<ClassType*>(req->data);
    (obj->*member_ptr)(req);
}

#undef USING_CLASS_TYPE
}  // namespace ryu::uv_callbacks