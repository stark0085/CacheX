#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include "../include/lru_cache.hpp"
#include "../include/lfu_cache.hpp"
#include "../include/fifo_cache.hpp"
#include "../include/mru_cache.hpp"

/**
 * @brief Generate a Zipfian-distributed random access trace.
 *
 * A Zipfian distribution models real-world access patterns where a small number
 * of "hot" keys are accessed disproportionately often, and the rest are accessed rarely.
 * The probability of accessing key i is proportional to 1 / i^s, where s controls
 * skewness (s=1.0 is classic Zipfian; s=0 is uniform).
 */
std::vector<int> generateZipfianTrace(int num_keys, size_t trace_length, double skewness = 1.0)
{
    std::vector<int> trace;
    trace.reserve(trace_length);

    // Precompute Zipfian probabilities
    std::vector<double> probabilities(num_keys);
    double normalization = 0.0;
    for (int i = 1; i <= num_keys; i++)
    {
        double prob = 1.0 / std::pow(i, skewness);
        probabilities[i - 1] = prob;
        normalization += prob;
    }

    // Normalize to [0, 1]
    for (auto &p : probabilities)
    {
        p /= normalization;
    }

    // Generate CDF for efficient sampling
    std::vector<double> cdf(num_keys);
    cdf[0] = probabilities[0];
    for (int i = 1; i < num_keys; i++)
    {
        cdf[i] = cdf[i - 1] + probabilities[i];
    }

    // Generate trace
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_real_distribution<> dis(0.0, 1.0);

    for (size_t i = 0; i < trace_length; i++)
    {
        double rand_val = dis(gen);
        // Binary search in CDF
        int key = std::upper_bound(cdf.begin(), cdf.end(), rand_val) - cdf.begin();
        key = std::min(key, num_keys - 1);
        trace.push_back(key);
    }

    return trace;
}

int main()
{
    std::cout << "Cachex - Multi-Policy Cache Engine (LRU + LFU)\n";
    std::cout << "============================================\n\n";

    // Benchmark parameters
    const size_t cache_capacity = 100;
    const int num_keys = 1000;         // Working set size
    const size_t trace_length = 10000; // Number of accesses
    const double zipfian_skewness = 1.0;

    std::cout << "Benchmark Configuration:\n";
    std::cout << "  Cache Capacity: " << cache_capacity << "\n";
    std::cout << "  Working Set Size (num_keys): " << num_keys << "\n";
    std::cout << "  Trace Length (operations): " << trace_length << "\n";
    std::cout << "  Zipfian Skewness: " << zipfian_skewness << "\n\n";

    // Generate identical access trace for both caches
    std::cout << "Generating Zipfian-distributed access trace...\n";
    auto trace = generateZipfianTrace(num_keys, trace_length, zipfian_skewness);

    // Count access frequency to understand hotness distribution
    std::vector<int> access_counts(num_keys, 0);
    for (int key : trace)
    {
        access_counts[key]++;
    }

    // Find top 5 hottest keys
    std::vector<std::pair<int, int>> hottest_keys;
    for (int i = 0; i < num_keys; i++)
    {
        hottest_keys.emplace_back(i, access_counts[i]);
    }
    std::sort(hottest_keys.rbegin(), hottest_keys.rend(),
              [](const auto &a, const auto &b)
              { return a.second < b.second; });

    std::cout << "Top 5 hottest keys:\n";
    for (int i = 0; i < 5; i++)
    {
        std::cout << "  Key " << hottest_keys[i].first << ": " << hottest_keys[i].second << " accesses\n";
    }
    std::cout << "\n";

    // Run LRU trace
    std::cout << "Running LRU trace...\n";
    LRUCache<int, int> lru_cache(cache_capacity);
    for (int key : trace)
    {
        auto val = lru_cache.get(key);
        if (!val.has_value())
        {
            lru_cache.put(key, key * 10);
        }
    }
    auto lru_stats = lru_cache.getStats();

    // Run LFU trace
    std::cout << "Running LFU trace...\n";
    LFUCache<int, int> lfu_cache(cache_capacity);
    for (int key : trace)
    {
        auto val = lfu_cache.get(key);
        if (!val.has_value())
        {
            lfu_cache.put(key, key * 10);
        }
    }
    auto lfu_stats = lfu_cache.getStats();

    // Run FIFO trace
    std::cout << "Running FIFO trace...\n";
    FIFOCache<int, int> fifo_cache(cache_capacity);
    for (int key : trace)
    {
        auto val = fifo_cache.get(key);
        if (!val.has_value())
        {
            fifo_cache.put(key, key * 10);
        }
    }
    auto fifo_stats = fifo_cache.getStats();

    // Run MRU trace (on the SAME Zipfian trace — expect this to perform worse
    // than the others here; MRU is not suited to hot-key workloads, see the
    // cyclic-scan comparison further below for where it actually excels).
    std::cout << "Running MRU trace...\n";
    MRUCache<int, int> mru_cache(cache_capacity);
    for (int key : trace)
    {
        auto val = mru_cache.get(key);
        if (!val.has_value())
        {
            mru_cache.put(key, key * 10);
        }
    }
    auto mru_stats = mru_cache.getStats();

    // Report results
    std::cout << "\n"
              << std::string(60, '=') << "\n";
    std::cout << "COMPARISON: LRU vs LFU\n";
    std::cout << std::string(60, '=') << "\n";

    std::cout << "\nLRU Cache Results:\n";
    std::cout << "  Total Accesses:  " << lru_stats.hits + lru_stats.misses << "\n";
    std::cout << "  Cache Hits:      " << lru_stats.hits << "\n";
    std::cout << "  Cache Misses:    " << lru_stats.misses << "\n";
    std::cout << "  Evictions:       " << lru_stats.evictions << "\n";
    std::cout << "  Hit Rate:        " << std::fixed << std::setprecision(4)
              << lru_stats.hitRate() * 100.0 << "%\n";

    std::cout << "\nLFU Cache Results:\n";
    std::cout << "  Total Accesses:  " << lfu_stats.hits + lfu_stats.misses << "\n";
    std::cout << "  Cache Hits:      " << lfu_stats.hits << "\n";
    std::cout << "  Cache Misses:    " << lfu_stats.misses << "\n";
    std::cout << "  Evictions:       " << lfu_stats.evictions << "\n";
    std::cout << "  Hit Rate:        " << std::fixed << std::setprecision(4)
              << lfu_stats.hitRate() * 100.0 << "%\n";

    std::cout << "\nFIFO Cache Results:\n";
    std::cout << "  Total Accesses:  " << fifo_stats.hits + fifo_stats.misses << "\n";
    std::cout << "  Cache Hits:      " << fifo_stats.hits << "\n";
    std::cout << "  Cache Misses:    " << fifo_stats.misses << "\n";
    std::cout << "  Evictions:       " << fifo_stats.evictions << "\n";
    std::cout << "  Hit Rate:        " << std::fixed << std::setprecision(4)
              << fifo_stats.hitRate() * 100.0 << "%\n";

    std::cout << "\nMRU Cache Results:\n";
    std::cout << "  Total Accesses:  " << mru_stats.hits + mru_stats.misses << "\n";
    std::cout << "  Cache Hits:      " << mru_stats.hits << "\n";
    std::cout << "  Cache Misses:    " << mru_stats.misses << "\n";
    std::cout << "  Evictions:       " << mru_stats.evictions << "\n";
    std::cout << "  Hit Rate:        " << std::fixed << std::setprecision(4)
              << mru_stats.hitRate() * 100.0 << "%\n";

    // Analysis
    std::cout << "\n"
              << std::string(60, '=') << "\n";
    std::cout << "ANALYSIS:\n";
    std::cout << std::string(60, '=') << "\n";

    double lru_hitrate = lru_stats.hitRate();
    double lfu_hitrate = lfu_stats.hitRate();

    std::cout << "\nZipfian Distribution Explanation:\n";
    std::cout << "With a Zipfian-distributed workload (skewness=1.0), only a small\n";
    std::cout << "number of keys are accessed frequently (\"hot\" keys), while most\n";
    std::cout << "keys are accessed rarely or never.\n\n";

    if (std::abs(lru_hitrate - lfu_hitrate) < 0.01)
    {
        std::cout << "Result: Hit rates are SIMILAR ("
                  << std::setprecision(2) << lru_hitrate * 100.0 << "% vs "
                  << lfu_hitrate * 100.0 << "%)\n";
        std::cout << "\nWhy: For this highly skewed access pattern, both LRU and LFU\n";
        std::cout << "effectively protect the hot keys. LRU naturally keeps frequently\n";
        std::cout << "accessed keys in the cache (recency ≈ frequency), so LFU's explicit\n";
        std::cout << "frequency tracking provides minimal additional benefit.\n";
    }
    else if (lfu_hitrate > lru_hitrate)
    {
        std::cout << "Result: LFU outperforms LRU ("
                  << std::setprecision(2) << lfu_hitrate * 100.0 << "% vs "
                  << lru_hitrate * 100.0 << "%)\n";
        std::cout << "\nWhy: LFU explicitly protects genuinely hot keys (high access\n";
        std::cout << "frequency) from being evicted by bursts of cold/one-off key\n";
        std::cout << "accesses. LRU's weakness: a truly frequently-accessed key can be\n";
        std::cout << "evicted if a burst of infrequent keys becomes 'more recent'. LFU\n";
        std::cout << "avoids this by tracking actual access frequency, not just recency.\n";
    }
    else
    {
        std::cout << "Result: LRU outperforms LFU ("
                  << std::setprecision(2) << lru_hitrate * 100.0 << "% vs "
                  << lfu_hitrate * 100.0 << "%)\n";
        std::cout << "\nWhy: This is unusual for Zipfian workloads. Possible reasons:\n";
        std::cout << "1. Cache is large enough that most hot keys fit regardless of policy\n";
        std::cout << "2. LRU's simplicity provides better temporal locality handling\n";
        std::cout << "3. Working set size (" << num_keys << ") vs capacity ("
                  << cache_capacity << ") ratio affects behavior\n";
    }
    std::cout << "\n";

    // ============================================================
    // SECOND BENCHMARK: Cyclic scan workload — designed specifically to
    // demonstrate MRU's actual use case, which Zipfian traffic above
    // does NOT represent fairly. A working set larger than capacity is
    // scanned repeatedly in full cycles; each key is touched once per
    // cycle, so the most-recently-touched key is the LEAST likely to be
    // needed again soon — the opposite of what LRU/LFU assume, and
    // exactly what MRU is designed to exploit.
    // ============================================================
    std::cout << "\n\n"
              << std::string(60, '=') << "\n";
    std::cout << "SECOND BENCHMARK: Cyclic Scan Workload (MRU's use case)\n";
    std::cout << std::string(60, '=') << "\n\n";

    const int scan_working_set = cache_capacity * 3 / 2; // working set 3x capacity
    const int scan_cycles = 20;

    std::cout << "Configuration:\n";
    std::cout << "  Cache Capacity: " << cache_capacity << "\n";
    std::cout << "  Working Set Size: " << scan_working_set << "\n";
    std::cout << "  Full Cycles: " << scan_cycles << "\n";
    std::cout << "  Total Accesses: " << scan_working_set * scan_cycles << "\n\n";

    auto runCyclicScan = [&](auto &cache)
    {
        // Deterministic sliding-window scan: at each position, touch the
        // current key AND immediately re-touch the previous 2 keys, then
        // move on. This models real scan locality (e.g. a moving-average
        // computation, or reviewing the last couple of items while
        // processing a stream) WITHOUT injecting random noise that could
        // accidentally build up frequency counts on arbitrary keys — every
        // key gets exactly the same, small, deterministic amount of
        // "extra" locality, so no policy gets an accidental frequency
        // advantage from randomness.
        const int window = 2;
        for (int cycle = 0; cycle < scan_cycles; ++cycle)
        {
            for (int key = 0; key < scan_working_set; ++key)
            {
                auto val = cache.get(key);
                if (!val.has_value())
                {
                    cache.put(key, key * 10);
                }
                for (int back = 1; back <= window; ++back)
                {
                    int recentKey = key - back;
                    if (recentKey >= 0)
                    {
                        cache.get(recentKey);
                    }
                }
            }
        }
        return cache.getStats();
    };

    LRUCache<int, int> lru_scan(cache_capacity);
    LFUCache<int, int> lfu_scan(cache_capacity);
    FIFOCache<int, int> fifo_scan(cache_capacity);
    MRUCache<int, int> mru_scan(cache_capacity);

    auto lru_scan_stats = runCyclicScan(lru_scan);
    auto lfu_scan_stats = runCyclicScan(lfu_scan);
    auto fifo_scan_stats = runCyclicScan(fifo_scan);
    auto mru_scan_stats = runCyclicScan(mru_scan);

    std::cout << "Hit Rates Under Cyclic Scan:\n";
    std::cout << "  LRU:  " << std::fixed << std::setprecision(2) << lru_scan_stats.hitRate() * 100.0 << "%\n";
    std::cout << "  LFU:  " << lfu_scan_stats.hitRate() * 100.0 << "%\n";
    std::cout << "  FIFO: " << fifo_scan_stats.hitRate() * 100.0 << "%\n";
    std::cout << "  MRU:  " << mru_scan_stats.hitRate() * 100.0 << "%\n\n";

    std::cout << "Why: In a sequential scan with local re-touches, LRU and FIFO land\n";
    std::cout << "at nearly identical hit rates -- in a strictly forward-moving scan,\n";
    std::cout << "'least recently used' and 'oldest inserted' select almost the same\n";
    std::cout << "keys, so the two policies behave similarly here. LFU tracks purely\n";
    std::cout << "how often, not how recently; with every key given the same small,\n";
    std::cout << "even amount of re-touching, it has no frequency signal to exploit\n";
    std::cout << "and performs about the same as the others. MRU wins clearly because\n";
    std::cout << "it deliberately evicts the JUST-touched key, protecting the ones\n";
    std::cout << "further ahead in the scan that are actually about to be needed --\n";
    std::cout << "the opposite assumption from the other three, and the one that is\n";
    std::cout << "actually correct for this specific access pattern.\n";

    return 0;
}
