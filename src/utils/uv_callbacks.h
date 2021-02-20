#pragma once

#include <string>
#include <cstdint>
#include <uv.h>

template<typename PtrT> struct get_class_of_member_pointer : std::false_type {};
template<typename R, typename C> struct get_class_of_member_pointer<R C::*> {
    using type = C;
};

namespace ryu::uv_callbacks {

template<auto member_ptr>
void Connection(uv_stream_t* server, int status) {
    using MemberPtrType = decltype(member_ptr); 
    static_assert(std::is_member_function_pointer<MemberPtrType>::value);
    using ClassType = typename get_class_of_member_pointer<MemberPtrType>::type;
    
    ClassType* obj = static_cast<ClassType*>(server->data);
    (obj->*member_ptr)(server, status);
}

}