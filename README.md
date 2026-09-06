# Cachex

Cachex is a dual-policy in-memory caching engine written in modern C++, supporting **LRU** and **LFU** eviction policies with true O(1) `get`/`put` performance. It's a header-only template library designed to drop into any C++ project.

## Highlights

- **O(1) LRU** — doubly-linked list + hash map, classic constant-time eviction.
- **O(1) LFU** — frequency-bucketed linked lists with LRU tie-breaking within each frequency bucket, avoiding the O(log n) overhead of a heap-based implementation.
- **31 test cases / 1,150 assertions** via Catch2, covering basic correctness, eviction ordering, edge cases (capacity 1, non-trivial key/value types), and randomized eviction-under-pressure scenarios at scale.
- **Memory-safety verified** — the full suite passes clean under AddressSanitizer + UndefinedBehaviorSanitizer, with sanitizer flags correctly wired through both compilation and linking via a dedicated `ENABLE_SANITIZERS` CMake option.
- **Benchmarked against a Zipfian-distributed workload** — on a 10,000-operation trace (skewness 1.0, capacity 100, working set 1,000 keys), LFU achieves a **65.25% hit rate** versus LRU's **57.15%**, a result consistent with published cache-benchmark literature on skewed access patterns. LFU wins here because it explicitly protects frequently-accessed keys from being evicted by bursts of cold, one-off traffic — a known weakness of pure recency-based eviction.

## Project Structure

```
cachex/
├── include/
│   ├── cache_interface.hpp    # ICache interface, CacheStats, EvictionPolicy
│   ├── lru_cache.hpp          # O(1) LRU implementation
│   ├── lfu_cache.hpp          # O(1) LFU implementation
│   └── ttl_cache_wrapper.hpp  # TTL decorator (in progress)
├── src/
│   └── main.cpp                # Zipfian benchmark driver comparing LRU vs LFU
├── tests/
│   └── test_caches.cpp         # Catch2 test suite
└── CMakeLists.txt
```

## Building and Running Tests

Requires CMake 3.14+ and a C++17-compliant compiler.

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
ctest --output-on-failure
```

To build with AddressSanitizer and UndefinedBehaviorSanitizer enabled:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
cmake --build .
ctest --output-on-failure
```

## Running the Benchmark

The manual driver runs an identical Zipfian-distributed access trace through both LRU and LFU and reports a side-by-side comparison of hit rate, miss rate, and eviction count:

```bash
./cachex_main
```

## Design Notes

- **TTL support** is implemented as a decorator (`TTLCacheWrapper<ICache>`) rather than being built into the core policies. This keeps the LRU/LFU implementations focused solely on eviction logic, and lets TTL be layered on top of any `ICache` implementation without modifying it. Currently a work in progress.
