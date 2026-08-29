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
TEST_CASE("LFUCache stub test", "[lfu]") {
    LFUCache<int, int> cache(10);
    REQUIRE(cache.capacity() == 10);
}

TEST_CASE("ARCCache stub test", "[arc]") {
    ARCCache<int, int> cache(10);
    REQUIRE(cache.capacity() == 10);
}
