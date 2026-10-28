# Cachex

**A multi-policy in-memory caching engine in C++, exposed as a live network service with a real-time visualizer.**

Cachex implements four eviction policies from scratch (LRU, LFU, FIFO and MRU) and wraps them in a REST + WebSocket server that supports live, Redis-style policy switching. TTL-based expiry is added through a decorator. Everything is covered by a 50-case, 1,215-assertion Catch2 test suite that passes cleanly under AddressSanitizer and UndefinedBehaviorSanitizer.

---

## Highlights

- **Four eviction policies, from scratch:**
  - **LRU**: doubly-linked list + hash map, O(1) `get`/`put`/eviction of the least-recently-used key.
  - **LFU**: frequency-bucketed linked lists with LRU tie-breaking inside each bucket, giving O(1) `get`/`put` without the O(log n) cost of a heap.
  - **FIFO**: structurally identical to LRU, but `get()` never reorders the list, so only insertion order decides eviction.
  - **MRU**: evicts the *most* recently used key; deliberately built to exploit sequential-scan workloads where recency is an anti-signal.
- **TTL expiry via decorator**: `TTLCacheWrapper<ICache>` layers time-based expiry over any policy without touching its eviction logic. It combines lazy expiry (checked on every access) with an active background sweep thread (runs every 1000 ms), and notifies the server whenever it reclaims a key.
- **Live network service**: a C++ REST API (via `cpp-httplib`) alongside a WebSocket server (via IXWebSocket) that streams every hit, miss, set, eviction, expiry, removal, clear and policy change to connected clients.
- **Redis-style live policy switching**: `POST /policy` swaps the eviction algorithm *without* wiping the cache. Existing key-value pairs and aggregate stats are migrated into the new policy. Per-policy metadata (an LFU key's access count, an LRU key's recency position) is deliberately **not** translated, because it has no honest meaning under a different policy.
- **1,215 test assertions** across 50 test cases, all passing under ASan + UBSan. They cover correctness, eviction ordering, edge cases, and randomized eviction-under-pressure scenarios for all four policies.
- **Two independently verified benchmarks:**
  - **Zipfian-distributed traffic** (hot-key workload): LFU reaches a **65.25%** hit rate vs. LRU **57.15%**, FIFO **51.63%** and MRU **15.84%**, consistent with published cache-benchmark literature on skewed access patterns.
  - **Sequential-scan workload** (working set 1.5x capacity, with local re-touch windows): MRU reaches **87.70%**, ahead of LRU (66.44%), FIFO (66.44%) and LFU (65.34%). Policy choice is workload-dependent, not universally ranked.
- **A dark, purpose-built visualizer**: a dependency-free HTML/CSS/JS frontend showing live cache state, hit rate, hits/misses/evictions, and a scrolling event log, with a 4-way policy toggle and controls to `PUT`/`GET`/`DELETE`/clear the cache.

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

The REST server and WebSocket server run as separate threads inside a single process, sharing one active cache instance (guarded by a mutex) and one `EventBroadcaster`. Switching policy via `POST /policy` reads every entry and the current stats from the active cache, constructs a fresh instance under the new policy, and re-inserts everything. Data and aggregate stats survive the switch; per-policy recency/frequency metadata does not.

---

## Project Structure

```
cachex/
├── include/
│   ├── cache_interface.hpp    # ICache interface and CacheStats
│   ├── lru_cache.hpp          # O(1) LRU implementation
│   ├── lfu_cache.hpp          # O(1) LFU implementation
│   ├── fifo_cache.hpp         # FIFO implementation
│   ├── mru_cache.hpp          # MRU implementation
│   ├── ttl_cache_wrapper.hpp  # TTL decorator (lazy expiry + active sweep thread)
│   └── httplib.h              # vendored header-only HTTP library (cpp-httplib)
├── server/
│   └── event_broadcaster.hpp  # tracks WebSocket clients, broadcasts JSON events
├── src/
│   ├── main.cpp               # benchmark driver (Zipfian + cyclic-scan, all 4 policies)
│   └── server_main.cpp        # REST + WebSocket server entry point
├── web/
│   ├── index.html             # visualizer markup
│   ├── style.css              # dark theme, all styling
│   └── app.js                 # WebSocket client, REST calls, rendering
├── tests/
│   └── test_caches.cpp        # Catch2 test suite (50 test cases)
└── CMakeLists.txt
```

The build produces three executables, all placed in your build directory:

| Executable | Built from | What it is |
|---|---|---|
| `test_caches` | `tests/test_caches.cpp` | The Catch2 unit-test runner |
| `cachex_main` | `src/main.cpp` | The benchmark driver (Zipfian + cyclic scan) |
| `cachex_server` | `src/server_main.cpp` | The REST + WebSocket server |

---

## 1. Prerequisites

You need a C++17 compiler, CMake 3.14 or newer, Git, and Python 3 (Python is only used to serve the web UI).

**Ubuntu / Debian / WSL**

```bash
sudo apt update
sudo apt install -y build-essential cmake git python3
```

**macOS** (with [Homebrew](https://brew.sh))

```bash
xcode-select --install        # compiler toolchain
brew install cmake git python3
```

Check your versions:

```bash
g++ --version      # or: clang++ --version   (needs C++17 support, GCC 8+ / Clang 7+)
cmake --version    # must be 3.14 or newer
git --version
python3 --version
```

**Library dependencies are fetched automatically.** You do not install them by hand. During the first configure step, CMake's `FetchContent` downloads and builds:

| Library | Version | Used for |
|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | v3.4.0 | unit tests |
| [IXWebSocket](https://github.com/machinezone/IXWebSocket) | v11.4.5 | WebSocket server (built with TLS off) |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | vendored in `include/httplib.h` | REST server (nothing to install) |

> The **first build needs an internet connection** (it clones Catch2 and IXWebSocket from GitHub) and takes a few minutes. Later builds reuse the downloaded sources and are fast.

> Tested on Linux (GCC 13, CMake 3.28). macOS with Apple Clang should work the same way. Windows is not tested; use WSL.

---

## 2. Build

Run these from the project root (the folder that contains `CMakeLists.txt`):

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j4
```

This builds all three executables (`test_caches`, `cachex_main`, `cachex_server`). If `-j4` is rejected by an old CMake, use `cmake --build .` or `make -j4`.

All commands in the sections below are run **from inside the `build/` directory**.

To start over from scratch, delete the build folder and repeat the steps above:

```bash
cd ..
rm -rf build
```

---

## 3. Run the tests

### Option A: through CTest (recommended)

```bash
ctest --output-on-failure
```

Expected result:

```
100% tests passed, 0 tests failed out of 50
```

CTest registers every Catch2 test case separately, so you see 50 individual results.

### Option B: run the test binary directly

```bash
./test_caches
```

Expected result:

```
All tests passed (1215 assertions in 50 test cases)
```

Handy options for the direct runner:

```bash
./test_caches --list-tests          # list all 50 test cases
./test_caches "[mru]"               # run only the tests tagged [mru]  (also [lru], [lfu], [fifo], ...)
./test_caches "some test name"      # run one test case by name
./test_caches -s                    # show every passing assertion too
```

The tests use randomized inputs. Catch2 prints `Randomness seeded to: ...` at the top; pass `--rng-seed <that number>` to replay a run exactly.

### Option C: with AddressSanitizer + UndefinedBehaviorSanitizer

Use a separate build folder so your Release build stays untouched:

```bash
cd ..                                  # back to the project root
mkdir build-san && cd build-san
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..
cmake --build . -j4
ctest --output-on-failure              # or ./test_caches
```

This should still report all 50 test cases passing, with no sanitizer errors. (Sanitizers are only supported with GCC and Clang.)

---

## 4. Run the benchmarks

```bash
./cachex_main
```

It prints two independent comparisons across all four policies:

1. **Zipfian hot-key trace**: 10,000 operations, capacity 100, working set of 1,000 keys, skew 1.0, fixed seed (so results are reproducible). Expected hit rates: LRU 57.15%, LFU 65.25%, FIFO 51.63%, MRU 15.84%.
2. **Cyclic scan with local re-touches**: capacity 100, working set 150, 20 cycles (3,000 accesses). Expected hit rates: LRU 66.44%, LFU 65.34%, FIFO 66.44%, MRU 87.70%.

The scan benchmark is designed to give every policy an honest, non-degenerate chance at reuse, rather than a workload geometry that forces some policies to exactly 0%.

---

## 5. Run the server on its own

The server is a standalone executable. You do not need the web UI to use it.

```bash
./cachex_server
```

You should see:

```
REST server listening on http://localhost:8080
WebSocket server listening on ws://localhost:8081
```

Leave it running and open a **second terminal** to talk to it:

```bash
# store a key (optionally with a TTL in seconds; default is 60)
curl -X PUT -d "hello" "http://localhost:8080/cache/greeting"
curl -X PUT -d "short-lived" "http://localhost:8080/cache/temp?ttl=5"

# read it back (404 if absent or expired)
curl -i http://localhost:8080/cache/greeting

# list keys and see stats
curl http://localhost:8080/cache-keys
curl http://localhost:8080/stats

# switch the eviction policy live (LRU | LFU | FIFO | MRU); data is migrated
curl -X POST -d "LFU" http://localhost:8080/policy

# delete one key / clear everything
curl -X DELETE http://localhost:8080/cache/greeting
curl -X DELETE http://localhost:8080/cache
```

To watch the live event stream without the web UI, connect any WebSocket client to `ws://localhost:8081`, for example (needs Node.js):

```bash
npx wscat -c ws://localhost:8081
```

Then run the `curl` commands above from the other terminal and watch the JSON events arrive.

Stop the server with `Ctrl+C`.

---

## 6. Run the full stack (server + visualizer)

You need **two terminals**, both with the server and UI running at the same time.

**Terminal 1: start the server** (from `build/`):

```bash
./cachex_server
```

**Terminal 2: serve the visualizer** (from the project root):

```bash
cd web
python3 -m http.server 8090
```

Then open **http://localhost:8090** in a browser. The status dot in the top bar turns green once the WebSocket connects.

> The UI is hard-wired to `http://localhost:8080` (REST) and `ws://localhost:8081` (WebSocket), so the server must run on the same machine as your browser. Do not open `index.html` by double-clicking the file; serve it over HTTP as shown.

From the visualizer you can `PUT`/`GET`/`DELETE` keys, set a per-key TTL, fill the cache with 20 random keys to trigger real evictions, switch between all four policies live (existing data migrates rather than disappearing), and clear the cache. Every action streams to the event log and updates the cache grid in real time.

---

## API Reference

| Method | Route | Description |
|---|---|---|
| `GET` | `/cache/:key` | Retrieve a value. 404 if absent or expired. |
| `PUT` | `/cache/:key` | Insert or update. Request body is the value. Optional `?ttl=<seconds>` for a per-key TTL (default: 60 s). Returns 204. |
| `DELETE` | `/cache/:key` | Remove a single key. Returns 204. |
| `DELETE` | `/cache` | Clear the entire cache. Returns 204. |
| `GET` | `/cache-keys` | List every key currently in the cache (JSON array). |
| `GET` | `/stats` | Current policy, hits, misses, evictions, hit rate (JSON). |
| `POST` | `/policy` | Switch active policy. Body: `LRU`, `LFU`, `FIFO`, or `MRU`. Migrates existing data. |

WebSocket clients connecting to `ws://localhost:8081` receive a JSON event on every `set`, `hit`, `miss`, `eviction`, `expired`, `remove`, `clear`, and `policy_change`.

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `cmake: command not found` or CMake older than 3.14 | Install/upgrade CMake (see Prerequisites). |
| CMake fails while fetching Catch2 / IXWebSocket | The first configure needs internet access to GitHub. Check your connection/proxy and rerun `cmake ..`. |
| `Address already in use` / `WS server failed to listen` | Port 8081 is taken by another process. Stop it, or find it with `lsof -i :8081`. |
| Server prints "listening on :8080" but REST calls fail | Port 8080 is taken by another process (the REST thread does not report a bind failure). Free the port (`lsof -i :8080`) and restart. |
| Browser shows the page but the status dot stays red | The server is not running, or the page was opened as a file instead of through `python3 -m http.server`. |
| Port 8090 already in use for the UI | Serve on another port, e.g. `python3 -m http.server 9000`, and open that port instead. |
| A stale `cachex_server` is still running after Ctrl+C | `pkill -x cachex_server`, then start it again. |

---
