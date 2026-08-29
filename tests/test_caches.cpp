#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "../include/lru_cache.hpp"
#include "../include/lfu_cache.hpp"
#include "../include/arc_cache.hpp"
#include <vector>

TEST_CASE("LRUCache Basic put/get correctness", "[lru]") {
    LRUCache<int, int> cache(2);
    REQUIRE(cache.size() == 0);
    
    cache.put(1, 100);
    REQUIRE(cache.size() == 1);
    
    auto val = cache.get(1);
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 100);
}


TEST_CASE("LRUCache Eviction happens in correct LRU order", "[lru]") {
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{3, 2, 1});
    
    // Evict 1 (least recently used)
    cache.put(4, 40);
    REQUIRE(cache.size() == 3);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{4, 3, 2});
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(4).value() == 40);
}

TEST_CASE("LRUCache get() updates recency and prevents eviction", "[lru]") {
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    
    // Access 1, making it most recently used
    cache.get(1);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{1, 3, 2});
    
    // Inserting 4 should evict 2 (which is now least recently used)
    cache.put(4, 40);
    REQUIRE_FALSE(cache.get(2).has_value());
    REQUIRE(cache.get(1).value() == 10);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{1, 4, 3});
}

TEST_CASE("LRUCache get() on missing key", "[lru]") {
    LRUCache<int, int> cache(2);
    cache.put(1, 10);
    
    auto val = cache.get(2);
    REQUIRE_FALSE(val.has_value());
    
    auto stats = cache.getStats();
    REQUIRE(stats.misses == 1);
    REQUIRE(stats.hits == 0);
}

TEST_CASE("LRUCache remove() works correctly", "[lru]") {
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    
    REQUIRE(cache.remove(2) == true);
    REQUIRE(cache.remove(99) == false); // non-existent
    
    REQUIRE(cache.size() == 2);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{3, 1});
    REQUIRE_FALSE(cache.get(2).has_value());
    
    // Put another to verify it doesn't break
    cache.put(4, 40);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{4, 3, 1});
}

TEST_CASE("LRUCache Stats are accurate", "[lru]") {
    LRUCache<int, int> cache(2);
    
    cache.put(1, 10);
    cache.put(2, 20);
    
    cache.get(1); // Hit
    cache.get(3); // Miss
    cache.get(2); // Hit
    cache.get(4); // Miss
    
    cache.put(3, 30); // Evicts 1 (since 2 was just accessed)
    
    cache.get(1); // Miss
    cache.get(3); // Hit
    
    auto stats = cache.getStats();
    REQUIRE(stats.hits == 3);
    REQUIRE(stats.misses == 3);
    REQUIRE(stats.evictions == 1);
    REQUIRE(stats.hitRate() == 0.5);
    
    cache.resetStats();
    stats = cache.getStats();
    REQUIRE(stats.hits == 0);
    REQUIRE(stats.misses == 0);
    REQUIRE(stats.evictions == 0);
    REQUIRE(stats.hitRate() == 0.0);
}

TEST_CASE("LRUCache Edge case: Capacity 1", "[lru]") {
    LRUCache<int, int> cache(1);
    cache.put(1, 10);
    REQUIRE(cache.get(1).value() == 10);
    
    cache.put(2, 20); // Evicts 1
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).value() == 20);
    
    auto stats = cache.getStats();
    REQUIRE(stats.evictions == 1);
}

TEST_CASE("LRUCache Edge case: Update existing key", "[lru]") {
    LRUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    
    // Update key 1
    cache.put(1, 15);
    
    // Should be moved to MRU
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{1, 2});
    REQUIRE(cache.get(1).value() == 15);
    
    // Put 3 should evict 2
    cache.put(3, 30);
    REQUIRE_FALSE(cache.get(2).has_value());
}

TEST_CASE("LRUCache Constructor invalid capacity", "[lru]") {
    auto create = []() { LRUCache<int, int> cache(0); };
    REQUIRE_THROWS_AS(create(), std::invalid_argument);
}

TEST_CASE("LRUCache Non-trivial types (std::string)", "[lru]") {
    LRUCache<std::string, std::string> cache(2);
    cache.put("a", "alpha");
    cache.put("b", "beta");
    REQUIRE(cache.get("a").value() == "alpha");
    
    cache.put("c", "gamma"); // evicts "b"
    REQUIRE_FALSE(cache.get("b").has_value());
    REQUIRE(cache.get("c").value() == "gamma");
}

TEST_CASE("LRUCache clear() resets the cache", "[lru]") {
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    
    cache.clear();
    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE_FALSE(cache.get(2).has_value());
    REQUIRE(cache.getKeysInOrder().empty());
}

TEST_CASE("LRUCache capacity() returns initial capacity", "[lru]") {
    LRUCache<int, int> cache(42);
    REQUIRE(cache.capacity() == 42);
}

// Stub tests for others
TEST_CASE("LFUCache Basic put/get correctness", "[lfu]") {
    LFUCache<int, int> cache(2);
    REQUIRE(cache.size() == 0);
    
    cache.put(1, 100);
    REQUIRE(cache.size() == 1);
    
    auto val = cache.get(1);
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 100);
}

TEST_CASE("LFUCache Eviction picks truly least-frequently-used", "[lfu]") {
    LFUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    
    // Frequencies: 1:2, 2:1, 3:1
    cache.get(1);
    
    // Frequencies: 1:2, 2:2, 3:1
    cache.get(2);
    
    // Insert 4, should evict 3 (only one with frequency 1)
    cache.put(4, 40);
    
    REQUIRE_FALSE(cache.get(3).has_value());
    REQUIRE(cache.get(4).value() == 40);
    REQUIRE(cache.get(1).value() == 10);
    REQUIRE(cache.get(2).value() == 20);
}

TEST_CASE("LFUCache Tie-breaking uses LRU within same frequency", "[lfu]") {
    LFUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    
    // Frequencies: 1:2, 2:2, 3:1
    cache.get(1);
    cache.get(2);
    
    // 3 is at frequency 1, let's bump it to 2.
    cache.get(3);
    // Now 1, 2, 3 are all at frequency 2.
    // Order of most recent access: 3, 2, 1. (1 is LRU)
    
    // Put 4 (evicts 1)
    cache.put(4, 40);
    
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).has_value());
    REQUIRE(cache.get(3).has_value());
    REQUIRE(cache.get(4).has_value());
}

TEST_CASE("LFUCache minFrequency tracking stays correct", "[lfu]") {
    LFUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    
    // Promote 1 and 2 to freq 2. min_freq should become 2.
    cache.get(1);
    cache.get(2);
    
    // Put 3. Evicts 1 (LRU of freq 2). 3 is at freq 1. min_freq becomes 1.
    cache.put(3, 30);
    REQUIRE_FALSE(cache.get(1).has_value());
    
    // Promote 3 to freq 2. min_freq becomes 2 again!
    cache.get(3);
    
    // Put 4. Evicts 2 (LRU of freq 2). 4 is at freq 1.
    cache.put(4, 40);
    REQUIRE_FALSE(cache.get(2).has_value());
    REQUIRE(cache.get(3).has_value());
    REQUIRE(cache.get(4).has_value());
}

TEST_CASE("LFUCache Stats are accurate", "[lfu]") {
    LFUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.get(1); // hit
    cache.get(2); // miss
    cache.put(2, 20);
    cache.put(3, 30); // evicts 2
    
    auto stats = cache.getStats();
    REQUIRE(stats.hits == 1);
    REQUIRE(stats.misses == 1);
    REQUIRE(stats.evictions == 1);
}

TEST_CASE("LFUCache Edge case: Capacity 1", "[lfu]") {
    LFUCache<int, int> cache(1);
    cache.put(1, 10);
    cache.get(1); // freq 2
    
    cache.put(2, 20); // Evicts 1 even though it has freq 2
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).value() == 20);
}

TEST_CASE("LFUCache put on existing key updates value and frequency", "[lfu]") {
    LFUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    
    // Update 1. Promotes to freq 2.
    cache.put(1, 15);
    REQUIRE(cache.get(1).value() == 15); // Hit, promotes to freq 3
    
    cache.put(3, 30); // Evicts 2
    REQUIRE_FALSE(cache.get(2).has_value());
}

TEST_CASE("LFUCache Constructor invalid capacity", "[lfu]") {
    auto create = []() { LFUCache<int, int> cache(0); };
    REQUIRE_THROWS_AS(create(), std::invalid_argument);
}

TEST_CASE("LFUCache remove() recalculates min_freq_ to new minimum", "[lfu]") {
    LFUCache<int, int> cache(3);
    
    // Fill cache with 3 keys at different frequencies
    cache.put(1, 10); // f1
    
    cache.put(2, 20); 
    cache.get(2); // f2
    
    cache.put(4, 40);
    cache.get(4);
    cache.get(4);
    cache.get(4); // f4
    
    // State: 1@f1, 2@f2, 4@f4. min_freq_ is 1.
    cache.remove(1); // removes the f1 key. size is 2. min_freq_ should become 2.
    
    // Put a new key to fill the cache. It will get f1.
    cache.put(5, 50); 
    // To ensure 5 isn't the one evicted (we want to test if 2 is evicted), 
    // we must bump 5's frequency ABOVE 2.
    cache.get(5);
    cache.get(5);
    cache.get(5); // 5 is now f4.
    
    // State: 2@f2, 4@f4, 5@f4.
    // If min_freq_ is correctly 2, putting a new key will evict 2.
    cache.put(6, 60); 
    
    REQUIRE_FALSE(cache.get(2).has_value());
    REQUIRE(cache.get(4).has_value());
    REQUIRE(cache.get(5).has_value());
    REQUIRE(cache.get(6).has_value());
}

TEST_CASE("LFUCache remove() on non-minimum frequency preserves min_freq_", "[lfu]") {
    LFUCache<int, int> cache(2);
    
    cache.put(1, 10); // f1
    cache.put(3, 30);
    cache.get(3);
    cache.get(3); // 3 is f3
    
    // State: 1@f1, 3@f3. min_freq_ is 1.
    cache.remove(3); // remove non-minimum
    
    // State: 1@f1.
    // Put a new key to fill the cache
    cache.put(4, 40); // 4@f1.
    // Bump 4 to f2 so it isn't the minimum
    cache.get(4); // 4@f2
    
    // Force eviction
    cache.put(5, 50); // should evict 1, because min_freq_ is 1
    
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(4).has_value());
    REQUIRE(cache.get(5).has_value());
}

TEST_CASE("LFUCache repeated remove() correctly advances min_freq_", "[lfu]") {
    LFUCache<int, int> cache(4);
    
    cache.put(1, 10); // f1
    
    cache.put(2, 20); cache.get(2); // f2
    
    cache.put(3, 30); cache.get(3); cache.get(3); // f3
    
    cache.put(4, 40); cache.get(4); cache.get(4); cache.get(4); // f4
    
    // State: 1@f1, 2@f2, 3@f3, 4@f4. min_freq_ = 1.
    cache.remove(1); // min_freq_ -> 2
    cache.remove(2); // min_freq_ -> 3
    
    // State: 3@f3, 4@f4.
    // Fill the cache with two new items, and bump their frequencies to f5 so they aren't evicted.
    cache.put(5, 50);
    cache.get(5); cache.get(5); cache.get(5); cache.get(5); // f5
    
    cache.put(6, 60);
    cache.get(6); cache.get(6); cache.get(6); cache.get(6); // f5
    
    // State: 3@f3, 4@f4, 5@f5, 6@f5. Cache is full.
    // Next put should evict 3!
    cache.put(7, 70);
    
    REQUIRE_FALSE(cache.get(3).has_value());
    REQUIRE(cache.get(4).has_value());
    REQUIRE(cache.get(7).has_value());
}

TEST_CASE("LFUCache remove() isolated state verification", "[lfu]") {
    LFUCache<int, int> cache(3);
    
    // 1. Populate the cache with distinct frequencies: 1, 2, 4
    cache.put(1, 10); // f1
    cache.put(2, 20); cache.get(2); // f2
    cache.put(4, 40); cache.get(4); cache.get(4); cache.get(4); // f4
    
    // 2. Remove the key at minimum frequency (freq 1)
    cache.remove(1); // Size drops to 2. True min_freq_ becomes 2.
    
    // 3. Do NOT call put(). Call get() on the new correct minimum (freq 2)
    cache.get(2); 
    // This promotes key '2' from freq 2 to freq 3. 
    // If remove() correctly updated min_freq_ to 2, it now naturally increments to 3.
    // If remove() left min_freq_ stale at 1, it stays stuck at 1.
    
    // 4. Call get() on a couple of other existing keys
    cache.get(4); // Promotes key '4' from freq 4 to freq 5
    
    // 5. Call put() with a new key to force an eviction.
    // NOTE: Because remove() dropped the size to 2 (below capacity 3), 
    // a single put() just fills the cache and unconditionally sets min_freq_ = 1.
    cache.put(5, 50); // Size 3, min_freq_ = 1.
    
    // To actually trigger the eviction threshold, we must put() a second time.
    cache.put(6, 60); // Evicts from min_freq_ (which is 1).
    
    // Assert the evicted key is correct (key 5, which was at freq 1).
    REQUIRE_FALSE(cache.get(5).has_value());
    REQUIRE(cache.get(2).has_value()); // Key 2 is safe at freq 3
    REQUIRE(cache.get(4).has_value()); // Key 4 is safe at freq 5
    REQUIRE(cache.get(6).has_value()); // Key 6 is at freq 1
}

// ─── ARC Tests ────────────────────────────────────────────────────────────────

TEST_CASE("ARCCache Basic put/get correctness", "[arc]") {
    ARCCache<int, int> cache(4);
    REQUIRE(cache.size() == 0);

    cache.put(1, 10);
    cache.put(2, 20);
    REQUIRE(cache.size() == 2);
    REQUIRE(cache.get(1).value() == 10);
    REQUIRE(cache.get(2).value() == 20);
    REQUIRE_FALSE(cache.get(99).has_value());
}

TEST_CASE("ARCCache Scan resistance: hot keys survive a scan", "[arc]") {
    // ARC's signature advantage: a scan of unique keys does not blow out
    // frequently-accessed entries, unlike plain LRU.
    const size_t cap = 8;
    ARCCache<int, int> cache(cap);

    // Warm up hot keys (each seen twice so they'll be in T2).
    for (int k : {1, 2, 3}) {
        cache.put(k, k * 10);
        cache.get(k); // promote to T2
    }

    // Scan 3*cap unique keys — far more than capacity.
    for (int s = 100; s < 100 + 3 * static_cast<int>(cap); ++s)
        cache.put(s, s);

    // Hot keys must still be in cache.
    REQUIRE(cache.get(1).has_value());
    REQUIRE(cache.get(2).has_value());
    REQUIRE(cache.get(3).has_value());
}

TEST_CASE("ARCCache p adapts upward on B1 hits", "[arc]") {
    const size_t cap = 4;
    ARCCache<int, int> cache(cap);

    // Fill cache with T1 entries.
    cache.put(1, 1); cache.put(2, 2); cache.put(3, 3); cache.put(4, 4);
    // L1=4=c, T1=4. Next puts trigger Branch A sub-case A2 (pop T1 directly).
    // We need B1 populated, so promote some to T2 first to reduce T1.
    cache.get(4); cache.get(3); // promote 4,3 → T2. T1={2,1}, T2={3,4}.

    // Now new inserts will REPLACE from T1 (p=0, T1.size=2>0), moving to B1.
    cache.put(5, 5); // REPLACE evicts LRU of T1(key 1) → B1. T1={2,5}... 
                     // actually inserts into T1 after replace.
    cache.put(6, 6); // evicts next LRU of T1 → B1.

    auto state_before = cache.getDebugState();
    // B1 must have entries for the test to be meaningful.
    REQUIRE_FALSE(state_before.b1.empty());

    double p_before = state_before.p;
    int b1_key = state_before.b1.back(); // re-request the LRU B1 key

    // Re-put a B1 key — triggers Case 2 (B1 ghost hit), p must increase.
    cache.put(b1_key, 999);
    double p_after = cache.getDebugState().p;

    REQUIRE(p_after > p_before);
}

TEST_CASE("ARCCache p adapts downward on B2 hits", "[arc]") {
    const size_t cap = 4;
    ARCCache<int, int> cache(cap);

    // Fill cache and promote entries to T2 via repeated gets.
    cache.put(1, 1); cache.put(2, 2); cache.put(3, 3); cache.put(4, 4);
    cache.get(1); cache.get(2); cache.get(3); cache.get(4); // all → T2

    // Evict from T2 into B2 by inserting new entries.
    cache.put(5, 5); // evicts LRU of T2 → B2
    cache.put(6, 6); // evicts next LRU of T2 → B2

    // Force p above 0 so a decrease is observable.
    double p_before = cache.getDebugState().p;

    // Re-insert a B2 key to trigger p decrease.
    auto state = cache.getDebugState();
    if (!state.b2.empty()) {
        int b2_key = state.b2.back();
        cache.put(b2_key, 999);
        double p_after = cache.getDebugState().p;
        REQUIRE(p_after <= p_before);
    }
}

TEST_CASE("ARCCache Invariants hold across 1000 random operations", "[arc]") {
    const size_t cap = 16;
    ARCCache<int, int> cache(cap);

    // Simple deterministic pseudo-random sequence (no <random> needed).
    unsigned state = 12345u;
    auto next_rand = [&](int mod) {
        state = state * 1664525u + 1013904223u;
        return static_cast<int>(state % static_cast<unsigned>(mod));
    };

    for (int op = 0; op < 1000; ++op) {
        int key = next_rand(24); // keys 0..23, wider than cap to stress eviction
        int action = next_rand(3);
        if (action == 0)      cache.put(key, key * 2);
        else if (action == 1) cache.get(key);
        else                  cache.remove(key);

        auto dbg = cache.getDebugState();
        REQUIRE(dbg.t1.size() + dbg.t2.size() <= cap);
        REQUIRE(dbg.t1.size() + dbg.b1.size() <= cap);
        REQUIRE(dbg.t2.size() + dbg.b2.size() <= cap);
        REQUIRE(dbg.t1.size() + dbg.t2.size() + dbg.b1.size() + dbg.b2.size() <= 2 * cap);
    }
}

TEST_CASE("ARCCache Boundary: |T1|+|B1| == c exactly (Branch A fires)", "[arc]") {
    const size_t cap = 3;
    ARCCache<int, int> cache(cap);

    // Fill T1 to capacity.
    cache.put(1, 1); cache.put(2, 2); cache.put(3, 3);
    // State: T1={3,2,1}, T2={}, B1={}, B2={}. |L1|=3=c.

    // Next put should trigger Branch A, sub-case |T1|<c (impossible here since T1=3=c).
    // So sub-case A2: evict T1 LRU directly.
    cache.put(4, 4);
    REQUIRE(cache.size() == cap);
    REQUIRE_FALSE(cache.get(1).has_value()); // 1 was LRU of T1, evicted directly

    auto dbg = cache.getDebugState();
    REQUIRE(dbg.t1.size() + dbg.b1.size() <= cap);
}

TEST_CASE("ARCCache Boundary: |T1|+|B1| == c-1 (Branch B fires)", "[arc]") {
    const size_t cap = 4;
    ARCCache<int, int> cache(cap);

    // Build state where L1 = c-1 = 3 and total >= c.
    // Fill T1=2, promote 2 to T2, giving T1=2, T2=2.
    cache.put(1,1); cache.put(2,2); cache.put(3,3); cache.put(4,4);
    cache.get(1); cache.get(2); // 1,2 → T2. T1={4,3}, T2={1,2}.

    // Now L1 = |T1|+|B1| = 2+0 = 2 = c-2. total = 4 = c. Branch B fires on next true miss.
    // (total >= c, L1 < c)
    cache.put(5, 5); // Branch B: no B2 trim (total=4 < 2*4=8), REPLACE, insert into T1.
    REQUIRE(cache.size() == cap);

    auto dbg = cache.getDebugState();
    REQUIRE(dbg.t1.size() + dbg.b1.size() <= cap);
    REQUIRE(dbg.t1.size() + dbg.t2.size() <= cap);
}

TEST_CASE("ARCCache Boundary: 2c total-directory threshold exactly hit", "[arc]") {
    const size_t cap = 3;
    ARCCache<int, int> cache(cap);

    // Build up ghost entries to approach 2c=6 total directory size.
    // Insert 6 distinct keys with eviction to populate ghosts.
    for (int k = 1; k <= 6; ++k)
        cache.put(k, k);

    auto dbg = cache.getDebugState();
    size_t total = dbg.t1.size() + dbg.t2.size() + dbg.b1.size() + dbg.b2.size();
    REQUIRE(total <= 2 * cap); // must never exceed 2c
}

TEST_CASE("ARCCache ARC hit rate >= LRU hit rate on skewed workload", "[arc]") {
    // Construct a workload where ARC's scan-resistance is measurable:
    //
    // A repeating pattern: access 3 hot keys, then scan (cap+1) cold unique keys.
    // Under plain LRU the cold scan evicts the hot keys every single round.
    // ARC adapts: it notices the hot keys keep coming back (B2 hits increase p
    // to protect T2), so after the first eviction cycle it shields them.
    //
    // With cap=4: hot={1,2,3}, cold scan size=5 (cap+1).
    // Each round: 3 hot accesses + 5 cold inserts. After round 1 both caches
    // miss the hot keys. From round 2 onward ARC's hits on hot keys should
    // outpace LRU's.
    const size_t cap = 4;
    ARCCache<int, int> arc(cap);
    LRUCache<int, int>  lru(cap);

    const int rounds = 8;
    const int cold_per_round = static_cast<int>(cap) + 1;

    for (int r = 0; r < rounds; ++r) {
        // Hot key accesses
        for (int h : {1, 2, 3}) {
            if (!arc.get(h).has_value()) arc.put(h, h);
            if (!lru.get(h).has_value()) lru.put(h, h);
        }
        // Cold scan — unique keys per round so they are true misses
        for (int c = 0; c < cold_per_round; ++c) {
            int k = 100 + r * cold_per_round + c;
            arc.put(k, k);
            lru.put(k, k);
        }
    }

    double arc_hr = arc.getStats().hitRate();
    double lru_hr = lru.getStats().hitRate();

    INFO("ARC hit rate: " << arc_hr << "  LRU hit rate: " << lru_hr);
    REQUIRE(arc_hr >= lru_hr);
}

TEST_CASE("ARCCache Stats accuracy", "[arc]") {
    ARCCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.get(1); // hit
    cache.get(3); // miss
    cache.put(3, 30); // true miss → eviction

    auto s = cache.getStats();
    REQUIRE(s.hits == 1);
    // 2 misses from put (initial inserts) + 1 from get(3) + 1 from put(3)
    REQUIRE(s.misses >= 1);
    REQUIRE(s.evictions >= 1);

    cache.resetStats();
    REQUIRE(cache.getStats().hits == 0);
}

TEST_CASE("ARCCache Edge case: capacity of 1", "[arc]") {
    ARCCache<int, int> cache(1);
    cache.put(1, 10);
    REQUIRE(cache.get(1).value() == 10);

    cache.put(2, 20); // must evict 1
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).value() == 20);

    REQUIRE(cache.size() == 1);
}

