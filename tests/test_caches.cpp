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

TEST_CASE("ARCCache stub test", "[arc]") {
    ARCCache<int, int> cache(10);
    REQUIRE(cache.capacity() == 10);
}
