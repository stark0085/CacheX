#pragma once

#include "cache_interface.hpp"
#include <list>
#include <unordered_map>
#include <stdexcept>
#include <vector>

/**
 * @brief Most-Recently-Used cache. Evicts the MOST recently accessed/inserted
 * key when full — the opposite of LRU's eviction target.
 *
 * Structurally identical to LRUCache (same MRU-at-front ordering, same
 * splice-on-access recency tracking) EXCEPT eviction pops from the FRONT
 * (most recent) instead of the BACK (least recent).
 *
 * Useful for workloads where recently-touched data is unlikely to be
 * needed again soon (e.g. a single sequential scan over a working set
 * larger than the cache) — NOT for typical "hot key" workloads like
 * Zipfian traffic, where MRU will perform poorly by design.
 */
template <typename Key, typename Value>
class MRUCache : public ICache<Key, Value>
{
public:
    explicit MRUCache(size_t capacity) : capacity_(capacity)
    {
        if (capacity == 0)
        {
            throw std::invalid_argument("Capacity must be greater than 0");
        }
    }

    bool put(const Key &key, const Value &value, Key *evictedKey = nullptr) override
    {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end())
        {
            it->second->second = value;
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
        }
        else
        {
            if (cache_map_.size() >= capacity_)
            {
                // Evict MOST recently used — front of the list.
                // This is the one line that differs from LRUCache::put(),
                // which evicts cache_list_.back() instead.
                const auto &mostRecent = cache_list_.front();
                if (evictedKey)
                {
                    *evictedKey = mostRecent.first;
                }
                cache_map_.erase(mostRecent.first);
                cache_list_.pop_front();
                stats_.evictions++;
            }
            cache_list_.emplace_front(key, value);
            cache_map_[key] = cache_list_.begin();
        }
        return true;
    }

    std::optional<Value> get(const Key &key) override
    {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end())
        {
            stats_.hits++;
            // Move accessed item to the front (MRU position) — same
            // recency-tracking as LRU. The difference is purely which
            // end gets evicted, not how recency is tracked.
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
            return it->second->second;
        }
        else
        {
            stats_.misses++;
            return std::nullopt;
        }
    }

    bool remove(const Key &key) override
    {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end())
        {
            cache_list_.erase(it->second);
            cache_map_.erase(it);
            return true;
        }
        return false;
    }

    void clear() override
    {
        cache_map_.clear();
        cache_list_.clear();
    }

    size_t size() const override
    {
        return cache_map_.size();
    }

    size_t capacity() const override
    {
        return capacity_;
    }

    CacheStats getStats() const override
    {
        return stats_;
    }

    void resetStats() override
    {
        stats_ = CacheStats{};
    }

    /**
     * @brief Returns keys most-recently-used first (i.e. the NEXT eviction
     * target is at index 0 — the opposite convention from LRUCache's
     * getKeysInOrder, where index 0 is also MRU but eviction targets the
     * OTHER end of this same vector).
     */
    std::vector<Key> getKeysInOrder() const
    {
        std::vector<Key> keys;
        keys.reserve(cache_list_.size());
        for (const auto &pair : cache_list_)
        {
            keys.push_back(pair.first);
        }
        return keys;
    }

    std::vector<std::pair<Key, Value>> getAllEntries() const override
    {
        std::vector<std::pair<Key, Value>> entries;
        entries.reserve(cache_list_.size());
        for (const auto &pair : cache_list_)
        {
            entries.push_back(pair);
        }
        return entries;
    }

    void setStats(const CacheStats &stats) override
    {
        stats_ = stats;
    }

private:
    size_t capacity_;
    CacheStats stats_;
    std::list<std::pair<Key, Value>> cache_list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map_;
};