cmake_minimum_required(VERSION 3.11)
project(Ryu)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -O0")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

add_subdirectory(third_party/fmt)
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/backward-cpp)

##
## TESTING
##
enable_testing()
find_package(GTest REQUIRED)

add_executable(bencode_test
    ${BACKWARD_ENABLE}
    bencode/bencode.cpp
    bencode/bencode_test.cpp
)
target_link_libraries(bencode_test PRIVATE fmt spdlog -ldw GTest::GTest GTest::Main)
gtest_discover_tests(bencode_test)
