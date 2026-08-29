# Cachex

Cachex is a dual-policy cache engine supporting LRU and LFU eviction policies. This is a header-only template library designed to be embedded easily into other C++ projects.

## Building and Running Tests

To build the project and run tests, you'll need CMake and a C++17 compliant compiler.

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest --output-on-failure
```

You can also run the manual test driver via `./cachex_main`.
