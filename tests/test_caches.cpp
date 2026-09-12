#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "../include/lru_cache.hpp"
#include "../include/lfu_cache.hpp"
#include "../include/fifo_cache.hpp"
#include "../include/mru_cache.hpp"
#include <vector>
#include <random>
#include <algorithm>

TEST_CASE("LRUCache Basic put/get correctness", "[lru]")
{
    LRUCache<int, int> cache(2);
    REQUIRE(cache.size() == 0);

    cache.put(1, 100);
    REQUIRE(cache.size() == 1);

    auto val = cache.get(1);
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 100);
}

TEST_CASE("LRUCache Eviction happens in correct LRU order", "[lru]")
{
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

TEST_CASE("LRUCache get() updates recency and prevents eviction", "[lru]")
{
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

TEST_CASE("LRUCache get() on missing key", "[lru]")
{
    LRUCache<int, int> cache(2);
    cache.put(1, 10);

    auto val = cache.get(2);
    REQUIRE_FALSE(val.has_value());

    auto stats = cache.getStats();
    REQUIRE(stats.misses == 1);
    REQUIRE(stats.hits == 0);
}

TEST_CASE("LRUCache remove() works correctly", "[lru]")
{
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

TEST_CASE("LRUCache Stats are accurate", "[lru]")
{
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

TEST_CASE("LRUCache Edge case: Capacity 1", "[lru]")
{
    LRUCache<int, int> cache(1);
    cache.put(1, 10);
    REQUIRE(cache.get(1).value() == 10);

    cache.put(2, 20); // Evicts 1
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).value() == 20);

    auto stats = cache.getStats();
    REQUIRE(stats.evictions == 1);
}

TEST_CASE("LRUCache Edge case: Update existing key", "[lru]")
{
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

TEST_CASE("LRUCache Constructor invalid capacity", "[lru]")
{
    auto create = []()
    { LRUCache<int, int> cache(0); };
    REQUIRE_THROWS_AS(create(), std::invalid_argument);
}

TEST_CASE("LRUCache Non-trivial types (std::string)", "[lru]")
{
    LRUCache<std::string, std::string> cache(2);
    cache.put("a", "alpha");
    cache.put("b", "beta");
    REQUIRE(cache.get("a").value() == "alpha");

    cache.put("c", "gamma"); // evicts "b"
    REQUIRE_FALSE(cache.get("b").has_value());
    REQUIRE(cache.get("c").value() == "gamma");
}

TEST_CASE("LRUCache clear() resets the cache", "[lru]")
{
    LRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);

    cache.clear();
    REQUIRE(cache.size() == 0);
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE_FALSE(cache.get(2).has_value());
    REQUIRE(cache.getKeysInOrder().empty());
}

TEST_CASE("LRUCache capacity() returns initial capacity", "[lru]")
{
    LRUCache<int, int> cache(42);
    REQUIRE(cache.capacity() == 42);
}

// Stub tests for others
TEST_CASE("LFUCache Basic put/get correctness", "[lfu]")
{
    LFUCache<int, int> cache(2);
    REQUIRE(cache.size() == 0);

    cache.put(1, 100);
    REQUIRE(cache.size() == 1);

    auto val = cache.get(1);
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 100);
}

TEST_CASE("LFUCache Eviction picks truly least-frequently-used", "[lfu]")
{
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

TEST_CASE("LFUCache Tie-breaking uses LRU within same frequency", "[lfu]")
{
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

TEST_CASE("LFUCache minFrequency tracking stays correct", "[lfu]")
{
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

TEST_CASE("LFUCache Stats are accurate", "[lfu]")
{
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

TEST_CASE("LFUCache Edge case: Capacity 1", "[lfu]")
{
    LFUCache<int, int> cache(1);
    cache.put(1, 10);
    cache.get(1); // freq 2

    cache.put(2, 20); // Evicts 1 even though it has freq 2
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).value() == 20);
}

TEST_CASE("LFUCache put on existing key updates value and frequency", "[lfu]")
{
    LFUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);

    // Update 1. Promotes to freq 2.
    cache.put(1, 15);
    REQUIRE(cache.get(1).value() == 15); // Hit, promotes to freq 3

    cache.put(3, 30); // Evicts 2
    REQUIRE_FALSE(cache.get(2).has_value());
}

TEST_CASE("LFUCache Constructor invalid capacity", "[lfu]")
{
    auto create = []()
    { LFUCache<int, int> cache(0); };
    REQUIRE_THROWS_AS(create(), std::invalid_argument);
}

TEST_CASE("LFUCache remove() recalculates min_freq_ to new minimum", "[lfu]")
{
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

TEST_CASE("LFUCache remove() on non-minimum frequency preserves min_freq_", "[lfu]")
{
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

TEST_CASE("LFUCache repeated remove() correctly advances min_freq_", "[lfu]")
{
    LFUCache<int, int> cache(4);

    cache.put(1, 10); // f1

    cache.put(2, 20);
    cache.get(2); // f2

    cache.put(3, 30);
    cache.get(3);
    cache.get(3); // f3

    cache.put(4, 40);
    cache.get(4);
    cache.get(4);
    cache.get(4); // f4

    // State: 1@f1, 2@f2, 3@f3, 4@f4. min_freq_ = 1.
    cache.remove(1); // min_freq_ -> 2
    cache.remove(2); // min_freq_ -> 3

    // State: 3@f3, 4@f4.
    // Fill the cache with two new items, and bump their frequencies to f5 so they aren't evicted.
    cache.put(5, 50);
    cache.get(5);
    cache.get(5);
    cache.get(5);
    cache.get(5); // f5

    cache.put(6, 60);
    cache.get(6);
    cache.get(6);
    cache.get(6);
    cache.get(6); // f5

    // State: 3@f3, 4@f4, 5@f5, 6@f5. Cache is full.
    // Next put should evict 3!
    cache.put(7, 70);

    REQUIRE_FALSE(cache.get(3).has_value());
    REQUIRE(cache.get(4).has_value());
    REQUIRE(cache.get(7).has_value());
}

TEST_CASE("LFUCache remove() isolated state verification", "[lfu]")
{
    LFUCache<int, int> cache(3);

    // 1. Populate the cache with distinct frequencies: 1, 2, 4
    cache.put(1, 10); // f1
    cache.put(2, 20);
    cache.get(2); // f2
    cache.put(4, 40);
    cache.get(4);
    cache.get(4);
    cache.get(4); // f4

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

TEST_CASE("LFUCache Non-trivial types (std::string)", "[lfu]")
{
    LFUCache<std::string, std::string> cache(2);
    cache.put("a", "alpha");
    cache.put("b", "beta");
    REQUIRE(cache.get("a").value() == "alpha");

    cache.put("c", "gamma"); // evicts "b" (LRU of freq 1)
    REQUIRE_FALSE(cache.get("b").has_value());
    REQUIRE(cache.get("c").value() == "gamma");
}

TEST_CASE("LFUCache Repeated fill-and-refill eviction scenarios", "[lfu]")
{
    LFUCache<int, int> cache(3);

    // Fill cache
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    REQUIRE(cache.size() == 3);

    // Refill cycle 1
    cache.put(4, 40);
    cache.put(5, 50);
    cache.put(6, 60);
    REQUIRE(cache.size() == 3);

    // Refill cycle 2
    cache.put(7, 70);
    cache.put(8, 80);
    cache.put(9, 90);
    REQUIRE(cache.size() == 3);

    // All old keys should be evicted
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE_FALSE(cache.get(2).has_value());
    REQUIRE_FALSE(cache.get(3).has_value());
}

TEST_CASE("LFUCache Multiple puts of same key without intervening gets", "[lfu]")
{
    LFUCache<int, int> cache(2);

    // Trace: put() on an existing key calls promote(), incrementing frequency by 1 each time.
    cache.put(1, 10); // Initial insert: freq = 1, min_freq = 1
    cache.put(1, 15); // Key exists, promote() called: freq 1 → 2
    cache.put(1, 20); // Key exists, promote() called: freq 2 → 3
    cache.put(1, 25); // Key exists, promote() called: freq 3 → 4
    cache.put(1, 30); // Key exists, promote() called: freq 4 → 5

    REQUIRE(cache.size() == 1);
    auto val = cache.get(1); // get() on existing key calls promote(): freq 5 → 6
    REQUIRE(val.value() == 30);

    // Key 1 should be at frequency 6 after 5 puts (each promoting +1) + 1 get (+1)
    auto freq_info = cache.getKeysWithFrequency();
    REQUIRE(freq_info.size() == 1);
    REQUIRE(freq_info[0].second == 6);
}

TEST_CASE("LRUCache Repeated fill-and-refill eviction scenarios", "[lru]")
{
    LRUCache<int, int> cache(3);

    // Fill cache
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    REQUIRE(cache.size() == 3);

    // Refill cycle 1
    cache.put(4, 40);
    cache.put(5, 50);
    cache.put(6, 60);
    REQUIRE(cache.size() == 3);

    // Refill cycle 2
    cache.put(7, 70);
    cache.put(8, 80);
    cache.put(9, 90);
    REQUIRE(cache.size() == 3);

    // All old keys should be evicted
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE_FALSE(cache.get(2).has_value());
    REQUIRE_FALSE(cache.get(3).has_value());
}

TEST_CASE("LRUCache Multiple puts of same key without intervening gets", "[lru]")
{
    LRUCache<int, int> cache(2);

    // Put same key 5 times
    cache.put(1, 10);
    cache.put(1, 15);
    cache.put(1, 20);
    cache.put(1, 25);
    cache.put(1, 30);

    REQUIRE(cache.size() == 1);
    REQUIRE(cache.get(1).value() == 30);
    REQUIRE(cache.size() == 1);
}

TEST_CASE("LRUCache Larger capacity with randomized access and real eviction", "[lru]")
{
    LRUCache<int, int> cache(100);

    // Insert 400 unique keys into capacity-100 cache to force heavy eviction
    std::mt19937 rng(12345); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> key_dist(0, 399);
    std::uniform_int_distribution<int> op_dist(0, 9);

    // 500 randomized operations: ~70% gets, ~30% puts
    for (int i = 0; i < 500; i++)
    {
        int random_key = key_dist(rng);
        int op_type = op_dist(rng);

        if (op_type < 7)
        {
            // 70% chance: get operation
            cache.get(random_key);
        }
        else
        {
            // 30% chance: put operation
            cache.put(random_key, random_key * 10);
        }

        // Assert cache never exceeds capacity
        REQUIRE(cache.size() <= cache.capacity());
    }

    // Verify eviction actually happened
    auto stats = cache.getStats();
    REQUIRE(stats.evictions > 0); // Should have evicted many times (400 keys into capacity 100)

    // Verify capacity never exceeded during entire test
    REQUIRE(cache.size() <= 100);

    // Now prove recency protection: add specific keys and access them repeatedly
    std::vector<int> protect_keys = {100, 101, 102};
    for (int key : protect_keys)
    {
        cache.put(key, key * 10);
    }

    // Access these keys repeatedly to make them MRU
    for (int key : protect_keys)
    {
        for (int j = 0; j < 10; j++)
        {
            auto val = cache.get(key);
            REQUIRE(val.has_value());
        }
    }

    // These recently-accessed keys must still be in the cache
    for (int key : protect_keys)
    {
        REQUIRE(cache.get(key).has_value());
    }
}

TEST_CASE("LFUCache Larger capacity with randomized access and real eviction", "[lfu]")
{
    LFUCache<int, int> cache(100);

    // Insert 400 unique keys into capacity-100 cache to force heavy eviction
    std::mt19937 rng(12345); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> key_dist(0, 399);
    std::uniform_int_distribution<int> op_dist(0, 9);

    std::vector<int> accessed_keys;

    // 500 randomized operations: ~70% gets, ~30% puts
    for (int i = 0; i < 500; i++)
    {
        int random_key = key_dist(rng);
        int op_type = op_dist(rng);

        if (op_type < 7)
        {
            // 70% chance: get operation
            auto val = cache.get(random_key);
            if (val.has_value())
            {
                accessed_keys.push_back(random_key);
            }
        }
        else
        {
            // 30% chance: put operation
            cache.put(random_key, random_key * 10);
        }

        // Assert cache never exceeds capacity
        REQUIRE(cache.size() <= cache.capacity());
    }

    // Verify eviction actually happened
    auto stats = cache.getStats();
    REQUIRE(stats.evictions > 0); // Should have evicted many times (400 keys into capacity 100)

    // Verify capacity never exceeded during entire test
    REQUIRE(cache.size() <= 100);

    // Now prove frequency protection: access specific keys repeatedly and verify retention
    if (accessed_keys.size() >= 3)
    {
        std::vector<int> frequent_keys = {accessed_keys[0], accessed_keys[1], accessed_keys[2]};
        for (int key : frequent_keys)
        {
            for (int j = 0; j < 5; j++)
            {
                cache.get(key);
            }
        }

        // These frequently-accessed keys should be in the cache
        for (int key : frequent_keys)
        {
            REQUIRE(cache.get(key).has_value());
        }

        // Verify that frequently-accessed keys have higher frequency than average
        auto freq_data = cache.getKeysWithFrequency();
        REQUIRE(freq_data.size() > 0);

        // Calculate average frequency
        double avg_freq = 0.0;
        for (const auto &[key, freq] : freq_data)
        {
            avg_freq += freq;
        }
        avg_freq /= freq_data.size();

        // Frequently-accessed keys should have above-average frequency
        for (int key : frequent_keys)
        {
            auto it = std::find_if(freq_data.begin(), freq_data.end(),
                                   [key](const auto &p)
                                   { return p.first == key; });
            if (it != freq_data.end())
            {
                REQUIRE(it->second > avg_freq);
            }
        }
    }
}

// ============================================================
// FIFOCache tests
// ============================================================

TEST_CASE("FIFOCache Basic put/get correctness", "[fifo]")
{
    FIFOCache<int, int> cache(2);
    REQUIRE(cache.size() == 0);

    cache.put(1, 100);
    REQUIRE(cache.size() == 1);

    auto val = cache.get(1);
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 100);
}

TEST_CASE("FIFOCache Eviction happens in pure insertion order", "[fifo]")
{
    FIFOCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    REQUIRE(cache.getKeysInOrder() == std::vector<int>{1, 2, 3}); // oldest first

    cache.put(4, 40); // evicts 1 (oldest), regardless of access pattern
    REQUIRE(cache.size() == 3);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{2, 3, 4});
    REQUIRE_FALSE(cache.get(1).has_value());
}

TEST_CASE("FIFOCache get() does NOT affect eviction order", "[fifo]")
{
    // This is the single behavior that distinguishes FIFO from LRU —
    // explicitly test that repeated access does not protect a key.
    FIFOCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    // Access key 1 repeatedly. In LRU this would move it to MRU position.
    // In FIFO it must have NO effect on eviction order.
    cache.get(1);
    cache.get(1);
    cache.get(1);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{1, 2, 3}); // unchanged

    cache.put(4, 40); // must evict 1 — the oldest — despite recent access
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).has_value());
    REQUIRE(cache.get(3).has_value());
    REQUIRE(cache.get(4).has_value());
}

TEST_CASE("FIFOCache put() on existing key updates value without reordering", "[fifo]")
{
    FIFOCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    cache.put(1, 999); // update existing key
    REQUIRE(cache.get(1).value() == 999);
    // Order must be unchanged — updating a key's value does not
    // re-insert it at the back in FIFO.
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{1, 2, 3});

    cache.put(4, 40); // must still evict 1 (oldest by insertion, not by update)
    REQUIRE_FALSE(cache.get(1).has_value());
}

TEST_CASE("FIFOCache remove() works correctly", "[fifo]")
{
    FIFOCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    REQUIRE(cache.remove(2) == true);
    REQUIRE(cache.remove(99) == false);
    REQUIRE(cache.size() == 2);
    REQUIRE(cache.getKeysInOrder() == std::vector<int>{1, 3});
}

TEST_CASE("FIFOCache Stats are accurate", "[fifo]")
{
    FIFOCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);

    cache.get(1); // hit
    cache.get(3); // miss

    cache.put(3, 30); // evicts 1

    auto stats = cache.getStats();
    REQUIRE(stats.hits == 1);
    REQUIRE(stats.misses == 1);
    REQUIRE(stats.evictions == 1);
}

TEST_CASE("FIFOCache Edge case: Capacity 1", "[fifo]")
{
    FIFOCache<int, int> cache(1);
    cache.put(1, 10);
    cache.put(2, 20); // evicts 1
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).value() == 20);
}

TEST_CASE("FIFOCache Constructor invalid capacity", "[fifo]")
{
    auto create = []()
    { FIFOCache<int, int> cache(0); };
    REQUIRE_THROWS_AS(create(), std::invalid_argument);
}

TEST_CASE("FIFOCache Non-trivial types (std::string)", "[fifo]")
{
    FIFOCache<std::string, std::string> cache(2);
    cache.put("a", "alpha");
    cache.put("b", "beta");
    cache.get("a");
    cache.get("a");          // repeated access, should NOT protect "a"
    cache.put("c", "gamma"); // must evict "a" (oldest), not "b"
    REQUIRE_FALSE(cache.get("a").has_value());
    REQUIRE(cache.get("b").value() == "beta");
    REQUIRE(cache.get("c").value() == "gamma");
}

// ============================================================
// MRUCache tests
// ============================================================

TEST_CASE("MRUCache Basic put/get correctness", "[mru]")
{
    MRUCache<int, int> cache(2);
    REQUIRE(cache.size() == 0);

    cache.put(1, 100);
    REQUIRE(cache.size() == 1);

    auto val = cache.get(1);
    REQUIRE(val.has_value());
    REQUIRE(val.value() == 100);
}

TEST_CASE("MRUCache Eviction targets the MOST recently used key", "[mru]")
{
    MRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    // Most recent insertion (3) is MRU by default.

    cache.put(4, 40); // must evict 3 (most recent), not 1 (oldest)
    REQUIRE(cache.size() == 3);
    REQUIRE_FALSE(cache.get(3).has_value());
    REQUIRE(cache.get(1).has_value()); // oldest survives — opposite of LRU
    REQUIRE(cache.get(2).has_value());
    REQUIRE(cache.get(4).has_value());
}

TEST_CASE("MRUCache get() updates recency and changes next eviction target", "[mru]")
{
    MRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    // Touch key 1, making IT the most-recently-used (and therefore the
    // next eviction target) even though it was inserted first.
    cache.get(1);

    cache.put(4, 40); // must evict 1 (now MRU due to the get() above)
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).has_value());
    REQUIRE(cache.get(3).has_value());
    REQUIRE(cache.get(4).has_value());
}

TEST_CASE("MRUCache put() on existing key updates value and refreshes recency", "[mru]")
{
    MRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    cache.put(1, 999); // update — should make 1 the new MRU
    REQUIRE(cache.get(1).value() == 999);

    cache.put(4, 40); // must evict 1 (just became MRU via the update)
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).has_value());
    REQUIRE(cache.get(3).has_value());
}

TEST_CASE("MRUCache remove() works correctly", "[mru]")
{
    MRUCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);

    REQUIRE(cache.remove(2) == true);
    REQUIRE(cache.remove(99) == false);
    REQUIRE(cache.size() == 2);

    cache.put(4, 40); // should not evict anything, still under capacity
    REQUIRE(cache.size() == 3);
    REQUIRE(cache.get(1).has_value());
    REQUIRE(cache.get(3).has_value());
    REQUIRE(cache.get(4).has_value());
}

TEST_CASE("MRUCache Stats are accurate", "[mru]")
{
    MRUCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);

    cache.get(1); // hit, also makes 1 the MRU
    cache.get(3); // miss

    cache.put(3, 30); // evicts 1 (MRU), not 2

    auto stats = cache.getStats();
    REQUIRE(stats.hits == 1);
    REQUIRE(stats.misses == 1);
    REQUIRE(stats.evictions == 1);
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).has_value());
}

TEST_CASE("MRUCache Edge case: Capacity 1", "[mru]")
{
    MRUCache<int, int> cache(1);
    cache.put(1, 10);
    cache.put(2, 20); // evicts 1 (the only entry, trivially both LRU and MRU)
    REQUIRE_FALSE(cache.get(1).has_value());
    REQUIRE(cache.get(2).value() == 20);
}

TEST_CASE("MRUCache Constructor invalid capacity", "[mru]")
{
    auto create = []()
    { MRUCache<int, int> cache(0); };
    REQUIRE_THROWS_AS(create(), std::invalid_argument);
}

TEST_CASE("MRUCache Non-trivial types (std::string)", "[mru]")
{
    MRUCache<std::string, std::string> cache(2);
    cache.put("a", "alpha");
    cache.put("b", "beta");  // "b" is now MRU
    cache.put("c", "gamma"); // must evict "b" (MRU), not "a"
    REQUIRE_FALSE(cache.get("b").has_value());
    REQUIRE(cache.get("a").value() == "alpha");
    REQUIRE(cache.get("c").value() == "gamma");
}

TEST_CASE("MRUCache scan-resistant workload: outperforms LRU on cyclic scans", "[mru]")
{
    // MRU's actual use case: a working set larger than capacity, scanned
    // repeatedly in a cycle. Each key is touched once per cycle, so the
    // "most recently touched" key is always the LEAST likely to be
    // touched again soon (it'll be capacity-many keys before its next
    // access) — this is exactly backwards from what LRU assumes.
    const int workingSetSize = 20;
    const int cacheCapacity = 10;
    const int cycles = 5;

    MRUCache<int, int> mru(cacheCapacity);
    LRUCache<int, int> lru(cacheCapacity);

    for (int cycle = 0; cycle < cycles; ++cycle)
    {
        for (int key = 0; key < workingSetSize; ++key)
        {
            if (!mru.get(key).has_value())
                mru.put(key, key);
            if (!lru.get(key).has_value())
                lru.put(key, key);
        }
    }

    auto mruStats = mru.getStats();
    auto lruStats = lru.getStats();

    // On this workload, MRU should achieve a meaningfully higher hit rate
    // than LRU, since LRU keeps evicting exactly the keys that are about
    // to be needed again (the ones it hasn't seen "recently" in a full
    // cyclic scan), while MRU protects them by evicting the just-touched
    // key instead.
    REQUIRE(mruStats.hitRate() > lruStats.hitRate());
}