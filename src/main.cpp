#include <iostream>
#include "../include/lru_cache.hpp"
#include "../include/lfu_cache.hpp"
#include "../include/arc_cache.hpp"

int main() {
    std::cout << "Cachex - Multi-Policy Cache Engine (Stub)\n";
    
    LRUCache<int, int> lru(10);
    LFUCache<int, int> lfu(10);
    ARCCache<int, int> arc(10);
    
    return 0;
}
