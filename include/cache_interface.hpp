#pragma once

#include <optional>
#include <cstddef>

/**
 * @brief Represents the available eviction policies for the cache engine.
 */
enum class EvictionPolicy {
    LRU,
    LFU
};

/**
 * @brief Statistics structure for cache performance metrics.
 */
struct CacheStats {
    size_t hits = 0;
    size_t misses = 0;
    size_t evictions = 0;

    /**
     * @brief Computes the hit rate of the cache.
     * @return The ratio of hits to total lookups (hits + misses). Returns 0.0 if no lookups have occurred.
     */
    double hitRate() const {
        size_t total = hits + misses;
        return total == 0 ? 0.0 : static_cast<double>(hits) / static_cast<double>(total);
    }

    bool operator==(const CacheStats& other) const {
        return hits == other.hits && misses == other.misses && evictions == other.evictions;
    }
};

/**
 * @brief Abstract base class for all cache implementations.
 *
 * @tparam Key Type of the keys stored in the cache.
 * @tparam Value Type of the values stored in the cache.
 */
template <typename Key, typename Value>
class ICache {
public:
    virtual ~ICache() = default;

    /**
     * @brief Inserts or updates a key-value pair in the cache.
     * 
     * If the cache is at capacity and a new key is inserted, an existing entry
     * will be evicted according to the cache's policy.
     *
     * @param key The key to insert or update.
     * @param value The value associated with the key.
     * @return true Always returns true (reserved for future error handling).
     */
    virtual bool put(const Key& key, const Value& value) = 0;

    /**
     * @brief Retrieves a value from the cache.
     * 
     * If the key is present, returns the value and updates the internal 
     * recency/frequency metadata according to the cache policy. 
     * If the key is absent, returns std::nullopt.
     *
     * @param key The key to look up.
     * @return std::optional<Value> The value if found, or std::nullopt if absent.
     */
    virtual std::optional<Value> get(const Key& key) = 0;

    /**
     * @brief Explicitly removes a key-value pair from the cache.
     *
     * @param key The key to remove.
     * @return bool True if the key existed and was successfully removed, false otherwise.
     */
    virtual bool remove(const Key& key) = 0;

    /**
     * @brief Clears all entries from the cache.
     */
    virtual void clear() = 0;

    /**
     * @brief Gets the current number of entries in the cache.
     *
     * @return size_t The number of cached items.
     */
    virtual size_t size() const = 0;

    /**
     * @brief Gets the maximum capacity of the cache.
     *
     * @return size_t The maximum number of items the cache can hold.
     */
    virtual size_t capacity() const = 0;

    /**
     * @brief Gets the current cache statistics.
     *
     * @return CacheStats The statistics containing hits, misses, and evictions.
     */
    virtual CacheStats getStats() const = 0;

    /**
     * @brief Resets the cache statistics (hits, misses, evictions) to zero.
     */
    virtual void resetStats() = 0;
};
