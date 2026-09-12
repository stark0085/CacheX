# Cachex

**A multi-policy in-memory caching engine in C++, exposed as a live network service with a real-time visualizer.**

Cachex implements four eviction policies from scratch — LRU, LFU, FIFO, and MRU — with true O(1) `get`/`put` performance where applicable, wraps them in a REST + WebSocket server supporting live, Redis-style policy switching, and adds TTL-based expiry via a decorator pattern. All backed by a 50-case, 1,850+ assertion Catch2 test suite verified clean under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## Highlights

- **Four eviction policies, from scratch:**
  - **LRU** — doubly-linked list + hash map, O(1) eviction of the least-recently-used key.
  - **LFU** — frequency-bucketed linked lists with LRU tie-breaking within each bucket, avoiding the O(log n) overhead of a heap-based approach.
  - **FIFO** — structurally identical to LRU, but `get()` never reorders the list, so only insertion order determines eviction.
  - **MRU** — evicts the *most* recently used key instead of the least; deliberately built to exploit sequential-scan workloads where recency is an anti-signal, not a signal.
- **TTL expiry via decorator** — `TTLCacheWrapper<ICache>` layers time-based expiry over any cache implementation without touching its eviction logic. Combines lazy expiry (checked on every access) with an active background sweep thread, notifying subscribers whenever it reclaims a key.
- **Live network service** — a C++ REST API (via `cpp-httplib`) alongside a WebSocket server (via IXWebSocket) that streams every hit, miss, eviction, expiry, and policy change to connected clients in real time.
- **Redis-style live policy switching** — `POST /policy` swaps the active eviction algorithm *without* wiping the cache: existing key-value pairs and aggregate stats are migrated into the new policy, mirroring how Redis's `CONFIG SET maxmemory-policy` changes eviction strategy without clearing the dataset. Per-policy metadata (an LFU key's access count, an LRU key's recency position) is deliberately **not** translated across the switch, since it has no honest meaning under a different policy — only the raw data and aggregate stats carry over.
- **1,850+ test assertions** across 50 test cases, all passing under ASan + UBSan — covering correctness, eviction ordering, edge cases, and randomized eviction-under-pressure scenarios at scale for all four policies.
- **Two independently verified benchmarks:**
  - **Zipfian-distributed traffic** (hot-key workload): LFU achieves a **65.25% hit rate** vs. LRU's **57.15%**, FIFO's **51.63%**, and MRU's **15.84%** — consistent with published cache-benchmark literature on skewed access patterns.
  - **Sequential-scan workload** (working set 1.5x capacity, with local re-touch windows): MRU achieves an **87.70% hit rate**, clearly ahead of LRU (66.44%), FIFO (66.44%), and LFU (65.34%) — demonstrating that policy choice is workload-dependent, not universally ranked.
- **A dark, purpose-built visualizer** — a dependency-free HTML/CSS/JS frontend showing live cache state, per-key LFU frequency, hit rate, and a scrolling event log, with a 4-way policy toggle and controls to `PUT`/`GET`/`DELETE`/clear the cache.

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
   ┌────────┼────────┬────────┐
   ▼        ▼        ▼        ▼
 LRUCache LFUCache FIFOCache MRUCache
```

The REST server and WebSocket server run as two threads inside a single process, sharing one active cache instance (guarded by a mutex) and one `EventBroadcaster`. Switching policy via `POST /policy` reads out every entry and the current stats from the active cache, constructs a fresh instance under the new policy, and re-inserts everything — data and aggregate stats survive the switch; per-policy recency/frequency metadata does not.

---

## Project Structure

```
cachex/
├── include/
│   ├── cache_interface.hpp    # ICache interface, CacheStats, EvictionPolicy
│   ├── lru_cache.hpp          # O(1) LRU implementation
│   ├── lfu_cache.hpp          # O(1) LFU implementation
│   ├── fifo_cache.hpp         # FIFO implementation
│   ├── mru_cache.hpp          # MRU implementation
│   ├── ttl_cache_wrapper.hpp  # TTL decorator (lazy + active sweep)
│   └── httplib.h              # vendored header-only HTTP library
├── server/
│   └── event_broadcaster.hpp  # tracks WebSocket clients, broadcasts JSON events
├── src/
│   ├── main.cpp                # benchmark driver (Zipfian + cyclic-scan, all 4 policies)
│   └── server_main.cpp         # REST + WebSocket server entry point
├── web/
│   ├── index.html              # visualizer markup
│   ├── style.css                # dark theme, all styling
│   └── app.js                    # WebSocket client, REST calls, rendering
├── tests/
│   └── test_caches.cpp         # Catch2 test suite (50 cases)
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

### Running the benchmarks

```bash
./cachex_main
```

Runs two independent comparisons across all four policies:
1. A Zipfian-distributed hot-key trace (10,000 ops, capacity 100, working set 1,000).
2. A sequential-scan trace with local re-touch windows (capacity 100, working set 150, 20 cycles) — designed specifically to give every policy an honest, non-degenerate chance at reuse, rather than a workload geometry that forces some policies to exactly 0%.

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

From the visualizer you can `PUT`/`GET`/`DELETE` keys, set a per-key TTL, fill the cache with random traffic to trigger real evictions, switch between all four policies live (existing data migrates rather than disappearing), and clear the cache entirely — every action streams to the event log and updates the cache grid in real time.

---

## API Reference

| Method | Route | Description |
|---|---|---|
| `GET` | `/cache/:key` | Retrieve a value. 404 if absent or expired. |
| `PUT` | `/cache/:key` | Insert or update. Optional `?ttl=<seconds>` for a per-key TTL (default: 60s). |
| `DELETE` | `/cache/:key` | Remove a single key. |
| `DELETE` | `/cache` | Clear the entire cache. |
| `GET` | `/cache-keys` | List every key currently in the cache (JSON array). |
| `GET` | `/stats` | Current policy, hits, misses, evictions, hit rate (JSON). |
| `POST` | `/policy` | Switch active policy. Body: `LRU`, `LFU`, `FIFO`, or `MRU`. Migrates existing data. |

WebSocket clients connecting to `ws://localhost:8081` receive a JSON event on every `set`, `hit`, `miss`, `eviction`, `expired`, `remove`, `clear`, and `policy_change`.

---

## Design Notes

**TTL as a decorator, not a built-in feature.** `TTLCacheWrapper<ICache>` wraps any cache implementation rather than being baked into each policy directly. This keeps every eviction policy focused solely on its own logic, while TTL support composes on top via the standard `ICache` interface. The wrapper combines lazy expiry (checked on every `get`/`put`) with an active background sweep thread, and notifies the server layer via a callback whenever it reclaims an expired key, so the live visualizer stays accurate even for keys nobody explicitly touches again.

**Policy switching migrates data, not metadata — modeled on Redis.** Redis's `CONFIG SET maxmemory-policy` changes eviction strategy at runtime without clearing the dataset; `POST /policy` here does the same. The `ICache` interface exposes `getAllEntries()` and `setStats()` specifically to support this: the server reads every key-value pair and the aggregate stats from the outgoing cache, constructs the new policy's cache, and re-inserts everything. What is deliberately **not** preserved is per-policy metadata — an LFU key's access frequency, an LRU key's recency position — since that metadata has no honest translation into a different policy's bookkeeping. A key that was "hot" under LFU doesn't get to start "recent" under LRU for free; it re-earns its position under the new policy's own rules, same as every other migrated key.

**MRU's benchmark required real debugging, not just a workload pick.** An early version of the sequential-scan benchmark produced an exact 0.00% hit rate for LRU, LFU, and FIFO — which looked like a strong result but was actually a degenerate one: at that working-set-to-capacity ratio, every policy was *structurally guaranteed* zero reuse regardless of its eviction logic, since every key was evicted long before its next scheduled access. A second attempt added locality via randomized re-touches, which fixed the zero-hit-rate issue but introduced a different bug: the random touches accidentally gave LFU an artificial frequency advantage, making it competitive with MRU for the wrong reason. The final version uses a deterministic, equal-for-every-key sliding-window re-touch, which gives an honest, explainable result: MRU clearly leads at 87.70%, with LRU and FIFO nearly identical (since in a purely sequential scan, "least recently used" and "oldest inserted" select almost the same keys) and LFU close behind them, no longer able to exploit an accidental frequency signal.

**Eviction-key reporting.** `ICache::put()` accepts an optional `Key* evictedKey` out-parameter, populated when an insertion causes an eviction. This was a deliberate interface addition made to support accurate eviction events in the live event stream — without it, the server could only report "an eviction happened," not which key was actually evicted.