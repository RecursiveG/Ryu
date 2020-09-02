cmake_minimum_required(VERSION 3.11)
project(Ryu VERSION 0.0.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -ggdb -O0")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

##
## Third party libraries
##
add_subdirectory(third_party/backward-cpp)
add_subdirectory(third_party/abseil)

##
## Supporting libraries
##
add_library(common STATIC)
target_sources(common PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/bencode.cpp)
target_include_directories(common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common)
target_link_libraries(common PUBLIC absl::strings PRIVATE absl::str_format)

##
## Testing binaries
##
add_executable(ryu
        src/ryu.cpp
        ${BACKWARD_ENABLE} src/torrent_file.cpp src/torrent_file.h)
target_link_libraries(ryu PRIVATE common -ldw absl::flags absl::flags_parse)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/experiments)
    add_executable(main
        ${CMAKE_CURRENT_SOURCE_DIR}/experiments/main.cpp
        ${BACKWARD_ENABLE}
    )
    target_link_libraries(main PRIVATE common -ldw)
endif()

##
## Unit tests
##
enable_testing()
find_package(GTest REQUIRED)

add_executable(bencode_test
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/bencode_test.cpp
    ${BACKWARD_ENABLE})
target_link_libraries(bencode_test PRIVATE common -ldw GTest::GTest GTest::Main)
gtest_discover_tests(bencode_test)

add_executable(result_test
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/result_test.cpp
        ${BACKWARD_ENABLE})
target_link_libraries(result_test PRIVATE common -ldw GTest::GTest GTest::Main)
gtest_discover_tests(result_test)

add_executable(network_test
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/network_test.cpp
        ${BACKWARD_ENABLE})
target_link_libraries(network_test PRIVATE common -ldw GTest::GTest GTest::Main)
gtest_discover_tests(network_test)
