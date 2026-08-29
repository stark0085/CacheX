#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>

#include "../include/lru_cache.hpp"
#include "../include/lfu_cache.hpp"
#include "../include/arc_cache.hpp"

TEST_CASE("LRUCache stub test", "[lru]") {
    LRUCache<int, int> cache(10);
    REQUIRE(cache.size() == 0);
}

TEST_CASE("LFUCache stub test", "[lfu]") {
    LFUCache<int, int> cache(10);
    REQUIRE(cache.size() == 0);
}

TEST_CASE("ARCCache stub test", "[arc]") {
    ARCCache<int, int> cache(10);
    REQUIRE(cache.size() == 0);
}
