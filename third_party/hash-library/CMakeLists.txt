add_library(hash-library STATIC)

target_sources(hash-library PRIVATE
    crc32.cpp
    digest.cpp
    keccak.cpp
    md5.cpp
    sha1.cpp
    sha3.cpp
    sha256.cpp)
target_include_directories(hash-library
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(hash-library-tests tests/tests.cpp)
target_link_libraries(hash-library-tests PRIVATE hash-library)

