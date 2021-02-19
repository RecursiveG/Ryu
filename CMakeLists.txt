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
include(FetchContent)
FetchContent_Declare(cpr
    GIT_REPOSITORY https://github.com/whoshuu/cpr.git
    GIT_TAG        e627e47727bfa13314dbe43418cf825097a2d38e)
FetchContent_Declare(result
    GIT_REPOSITORY https://github.com/RecursiveG/result.git
    GIT_TAG        8e8680449cc2764c1f6bb1696c348c54c3ef9441
)
FetchContent_Declare(abseil-cpp
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
    GIT_TAG        20200923.2
)
FetchContent_Declare(backward-cpp
    GIT_REPOSITORY https://github.com/bombela/backward-cpp.git
    GIT_TAG        v1.5
)
FetchContent_Declare(hash-library
    GIT_REPOSITORY https://github.com/stbrumme/hash-library.git
    GIT_TAG        hash_library_v8
)
FetchContent_MakeAvailable(cpr)
FetchContent_MakeAvailable(result)
FetchContent_MakeAvailable(abseil-cpp)
FetchContent_MakeAvailable(backward-cpp)
FetchContent_MakeAvailable(hash-library)

##
## Components
##
add_library(bencode STATIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/bencode.cpp)
target_include_directories(bencode PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(bencode PUBLIC result PRIVATE absl::str_format)

file(GLOB hash-library_SOURCE_FILES ${hash-library_SOURCE_DIR}/*.cpp)
add_library(hash-library STATIC ${hash-library_SOURCE_FILES})
target_include_directories(hash-library PUBLIC ${hash-library_SOURCE_DIR})

add_library(torrent_file STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/torrent_file.cpp)
target_link_libraries(torrent_file
    PUBLIC absl::strings absl::str_format bencode
    PRIVATE hash-library)

add_library(trackers STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/trackers.cpp)
target_link_libraries(trackers
    PUBLIC cpr::cpr bencode result
    PRIVATE absl::str_format)

##
## Tools
##
add_executable(torrent_info
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tools/torrent_info.cpp
    ${BACKWARD_ENABLE}
)
target_include_directories(torrent_info PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(torrent_info PRIVATE
    -ldw absl::flags absl::flags_parse absl::str_format
    bencode torrent_file trackers hash-library cpr::cpr
)

# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/experiments)
#     add_executable(main
#         ${CMAKE_CURRENT_SOURCE_DIR}/experiments/main.cpp
#         ${BACKWARD_ENABLE}
#     )
#     target_link_libraries(main PRIVATE common -ldw)
# endif()

##
## Unit tests
##
enable_testing()
find_package(GTest REQUIRED)

add_executable(bencode_test
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/bencode_test.cpp
    ${BACKWARD_ENABLE})
target_link_libraries(bencode_test PRIVATE bencode -ldw GTest::GTest GTest::Main)
gtest_discover_tests(bencode_test)

add_executable(network_test
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common/network_test.cpp
    ${BACKWARD_ENABLE})
target_link_libraries(network_test PRIVATE -ldw GTest::GTest GTest::Main absl::strings result)
gtest_discover_tests(network_test)

add_executable(ordered_map_test
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ordered_map_test.cpp
    ${BACKWARD_ENABLE})
target_link_libraries(ordered_map_test PRIVATE -ldw GTest::GTest GTest::Main)
gtest_discover_tests(ordered_map_test)

add_executable(hash_library_test ${hash-library_SOURCE_DIR}/tests/tests.cpp)
target_link_libraries(hash_library_test PRIVATE hash-library)
