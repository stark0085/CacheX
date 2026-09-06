cmake_minimum_required(VERSION 3.14)
project(Cachex VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address -fsanitize=undefined)
    endif()
endif()

# Fetch Catch2 for unit testing
include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Include directories
include_directories(include)

# Main executable for manual smoke-testing
add_executable(cachex_main src/main.cpp)
target_include_directories(cachex_main PRIVATE include)

# Test executable
enable_testing()
add_executable(test_caches tests/test_caches.cpp)
target_include_directories(test_caches PRIVATE include)
target_link_libraries(test_caches PRIVATE Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(test_caches)

# Server executable
add_executable(cachex_server src/server_main.cpp)
target_include_directories(cachex_server PRIVATE include)
target_link_libraries(cachex_server PRIVATE ixwebsocket)

# Fetch IXWebSocket for the WebSocket server layer
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v11.4.5
)
set(USE_TLS OFF CACHE BOOL "" FORCE)   # we don't need SSL for a local dev server
FetchContent_MakeAvailable(ixwebsocket)

# WebSocket server executable (separate binary, separate port)
add_executable(cachex_ws_server src/ws_server_main.cpp)
target_include_directories(cachex_ws_server PRIVATE include)
target_link_libraries(cachex_ws_server PRIVATE ixwebsocket)