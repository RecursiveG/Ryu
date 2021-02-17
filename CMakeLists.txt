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
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -ggdb -O0 -Wno-unused-parameter")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

## CPR flags
set(USE_SYSTEM_CURL YES)
set(USE_SYSTEM_GTEST YES)

##
## Third party libraries
##
add_subdirectory(third_party/backward-cpp)
add_subdirectory(third_party/abseil)
add_subdirectory(third_party/hash-library)
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/whoshuu/cpr.git GIT_TAG e627e47727bfa13314dbe43418cf825097a2d38e)
FetchContent_Declare(result
    GIT_REPOSITORY https://github.com/RecursiveG/result.git
    GIT_TAG        8e8680449cc2764c1f6bb1696c348c54c3ef9441
)
FetchContent_MakeAvailable(result)
FetchContent_MakeAvailable(cpr)

##
## Supporting libraries
##
add_library(common STATIC)
target_sources(common PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/bencode.cpp)
target_include_directories(common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils)
target_link_libraries(common PUBLIC absl::strings result PRIVATE absl::str_format)

##
## Testing binaries
##
add_executable(ryu
        src/ryu.cpp
        ${BACKWARD_ENABLE} src/torrent_file.cpp src/torrent_file.h src/os.h src/trackers.cpp)
target_link_libraries(ryu PRIVATE common -ldw absl::flags absl::flags_parse hash-library cpr::cpr)

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

add_executable(network_test
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/network_test.cpp
        ${BACKWARD_ENABLE})
target_link_libraries(network_test PRIVATE common -ldw GTest::GTest GTest::Main)
gtest_discover_tests(network_test)

add_executable(ordered_map_test
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ordered_map_test.cpp
    ${BACKWARD_ENABLE})
target_link_libraries(ordered_map_test PRIVATE -ldw GTest::GTest GTest::Main)
gtest_discover_tests(ordered_map_test)
