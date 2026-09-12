#pragma once

#include "cache_interface.hpp"
#include <list>
#include <unordered_map>
#include <stdexcept>
#include <vector>

/**
 * @brief First-In-First-Out cache. Evicts the OLDEST inserted key when full,
 * regardless of how recently or frequently it was accessed.
 *
 * Unlike LRU, get() does NOT reorder the list — only insertion order matters.
 * This is the single structural difference from LRUCache.
 */
template <typename Key, typename Value>
class FIFOCache : public ICache<Key, Value>
{
public:
    explicit FIFOCache(size_t capacity) : capacity_(capacity)
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
            // Key already exists: update value ONLY. Do NOT move it in the
            // list — insertion order is fixed at first-insert time in FIFO,
            // unlike LRU where an update also refreshes recency.
            it->second->second = value;
        }
        else
        {
            if (cache_map_.size() >= capacity_)
            {
                // Evict the oldest entry (front of the list, since we
                // push new entries to the back — see insertion below).
                const auto &oldest = cache_list_.front();
                if (evictedKey)
                {
                    *evictedKey = oldest.first;
                }
                cache_map_.erase(oldest.first);
                cache_list_.pop_front();
                stats_.evictions++;
            }
            // New entries go to the BACK; oldest sits at the FRONT.
            cache_list_.emplace_back(key, value);
            auto lastIt = cache_list_.end();
            --lastIt;
            cache_map_[key] = lastIt;
        }
        return true;
    }

    std::optional<Value> get(const Key &key) override
    {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end())
        {
            stats_.hits++;
            // No reordering — this is what makes it FIFO, not LRU.
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
     * @brief Returns keys in insertion order: oldest first, newest last.
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