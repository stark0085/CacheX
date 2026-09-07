# Cachex

**A multi-policy in-memory caching engine in C++, exposed as a live network service with a real-time visualizer.**

Cachex implements LRU and LFU eviction from scratch with true O(1) `get`/`put` performance, wraps them in a REST + WebSocket server for live policy switching and event streaming, and adds TTL-based expiry via a decorator pattern — all backed by a 31-case, 1,150-assertion Catch2 test suite verified clean under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## Highlights

- **O(1) LRU** — doubly-linked list + hash map, classic constant-time eviction.
- **O(1) LFU** — frequency-bucketed linked lists with LRU tie-breaking within each frequency bucket, avoiding the O(log n) overhead of a heap-based approach.
- **TTL expiry via decorator** — `TTLCacheWrapper<ICache>` layers time-based expiry over any cache implementation without touching its eviction logic. Combines lazy expiry (checked on every access) with an active background sweep thread, so expired entries are reclaimed even if nobody touches them again.
- **Live network service** — a C++ REST API (via `cpp-httplib`) alongside a WebSocket server (via IXWebSocket) that streams every hit, miss, eviction, and expiry event to connected clients in real time.
- **Live policy switching** — swap the active eviction policy (LRU ⇄ LFU) at runtime via `POST /policy`, no restart required.
- **1,150 test assertions**, 31 test cases, all passing under ASan + UBSan — covering correctness, eviction ordering, edge cases, and randomized eviction-under-pressure scenarios at scale.
- **Benchmarked against a Zipfian-distributed workload**: on a 10,000-operation trace (skewness 1.0, capacity 100, working set 1,000 keys), LFU achieves a **65.25% hit rate** versus LRU's **57.15%** — a result consistent with published cache-benchmark literature on skewed access patterns. LFU wins here because it explicitly protects frequently-accessed keys from being evicted by bursts of cold, one-off traffic, a known weakness of pure recency-based eviction.
- **A dark, purpose-built visualizer** — a dependency-free HTML/CSS/JS frontend showing live cache state, per-key LFU frequency, hit rate, and a scrolling event log, with controls to `PUT`/`GET`/`DELETE`/clear the cache and switch policy live.

---

## Architecture

```
Browser (web/)
   │  fetch()          WebSocket
   ▼                       ▼
REST API (:8080)     Event stream (:8081)
   │                       ▲
   └────────┬──────────────┘
            ▼
   TTLCacheWrapper<ICache>
            │
     ┌──────┴──────┐
     ▼             ▼
 LRUCache      LFUCache
```

The REST server and WebSocket server run as two threads inside a single process, sharing one active cache instance (guarded by a mutex) and one `EventBroadcaster`. Every cache mutation — a hit, a miss, an eviction, an expiry, a policy switch — is broadcast live to every connected WebSocket client, which is what drives the visualizer's real-time updates.

---

## Project Structure

```
cachex/
├── include/
│   ├── cache_interface.hpp    # ICache interface, CacheStats, EvictionPolicy
│   ├── lru_cache.hpp          # O(1) LRU implementation
│   ├── lfu_cache.hpp          # O(1) LFU implementation
│   ├── ttl_cache_wrapper.hpp  # TTL decorator (lazy + active sweep)
│   └── httplib.h              # vendored header-only HTTP library
├── server/
│   └── event_broadcaster.hpp  # tracks WebSocket clients, broadcasts JSON events
├── src/
│   ├── main.cpp                # Zipfian benchmark driver (LRU vs LFU)
│   └── server_main.cpp         # REST + WebSocket server entry point
├── web/
│   ├── index.html              # visualizer markup
│   ├── style.css                # dark theme, all styling
│   └── app.js                    # WebSocket client, REST calls, rendering
├── tests/
│   └── test_caches.cpp         # Catch2 test suite (31 cases, 1,150 assertions)
└── CMakeLists.txt
```

---

## Building

Requires CMake 3.14+ and a C++17-compliant compiler.

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

The first build will take a few minutes — CMake's `FetchContent` pulls and compiles Catch2 and IXWebSocket from source.

### Running tests

```bash
ctest --output-on-failure
```

To build with AddressSanitizer and UndefinedBehaviorSanitizer enabled:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
cmake --build .
ctest --output-on-failure
```

### Running the benchmark

```bash
./cachex_main
```

Runs an identical Zipfian-distributed access trace through both LRU and LFU and prints a side-by-side comparison of hit rate, miss rate, and eviction count.

---

## Running the Full Stack

**1. Start the server** (REST on `:8080`, WebSocket on `:8081`):

```bash
./cachex_server
```

**2. Serve the visualizer:**

```bash
cd web
python3 -m http.server 8090
```

**3. Open `http://localhost:8090`** in a browser.

From the visualizer you can `PUT`/`GET`/`DELETE` keys, set a per-key TTL, fill the cache with random traffic to trigger real evictions, switch between LRU and LFU live, and clear the cache entirely — every action streams to the event log and updates the cache grid in real time.

---

## API Reference

| Method | Route | Description |
|---|---|---|
| `GET` | `/cache/:key` | Retrieve a value. 404 if absent or expired. |
| `PUT` | `/cache/:key` | Insert or update. Optional `?ttl=<seconds>` for a per-key TTL (default: 60s). |
| `DELETE` | `/cache/:key` | Remove a single key. |
| `DELETE` | `/cache` | Clear the entire cache. |
| `GET` | `/stats` | Current policy, hits, misses, evictions, hit rate (JSON). |
| `POST` | `/policy` | Switch active policy. Body: `LRU` or `LFU`. Resets the cache. |

WebSocket clients connecting to `ws://localhost:8081` receive a JSON event on every `set`, `hit`, `miss`, `eviction`, `expired`, `remove`, `clear`, and `policy_change`.

---

## Design Notes

**TTL as a decorator, not a built-in feature.** `TTLCacheWrapper<ICache>` wraps any cache implementation rather than being baked into `LRUCache`/`LFUCache` directly. This keeps the core eviction policies focused solely on their own logic — recency or frequency — while TTL support composes on top via the standard `ICache` interface. The wrapper combines lazy expiry (checked on every `get`/`put`) with an active background sweep thread, and notifies the server layer via a callback whenever it reclaims an expired key, so the live visualizer stays accurate even for keys nobody explicitly touches again.

**Policy switching resets the cache, deliberately.** `POST /policy` constructs a fresh cache under the new policy rather than migrating existing entries. Migration wouldn't meaningfully preserve anything: a key's LRU recency has no honest translation into LFU's frequency count, or vice versa. Starting clean is a more accurate representation of what "switching eviction strategy" actually means than a migration that would quietly misrepresent each key's history.

**ARC (Adaptive Replacement Cache) was scoped and partially implemented** on a separate branch (`arc`), but deferred from the main engine after review surfaced a correctness risk in one of its boundary conditions that needed more validation time than was available. LRU and LFU were prioritized to ship a fully correct, thoroughly tested engine rather than a broader but less verified one.

**Eviction-key reporting.** `ICache::put()` accepts an optional `Key* evictedKey` out-parameter, populated when an insertion causes an eviction. This was a deliberate interface addition (not part of the original design) made to support accurate eviction events in the live event stream — without it, the server could only report "evictions happened," not which key was actually evicted.
