#pragma once

#include "cache_interface.hpp"
#include <unordered_map>
#include <list>
#include <stdexcept>
#include <vector>
#include <limits>

template <typename Key, typename Value>
class LFUCache : public ICache<Key, Value>
{
private:
    struct Node
    {
        Value value;
        size_t freq;
        typename std::list<Key>::iterator it;
    };

    size_t capacity_;
    CacheStats stats_;
    size_t min_freq_;
    std::unordered_map<Key, Node> cache_map_;
    std::unordered_map<size_t, std::list<Key>> freq_list_map_;

    void promote(const Key &key, Node &node)
    {
        size_t freq = node.freq;
        freq_list_map_[freq].erase(node.it);
        if (freq_list_map_[freq].empty())
        {
            freq_list_map_.erase(freq);
            if (min_freq_ == freq)
            {
                min_freq_++;
            }
        }
        node.freq = freq + 1;
        freq_list_map_[node.freq].push_front(key);
        node.it = freq_list_map_[node.freq].begin();
    }

public:
    explicit LFUCache(size_t capacity) : capacity_(capacity), min_freq_(0)
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
            it->second.value = value;
            promote(key, it->second);
            return true;
        }

        if (cache_map_.size() >= capacity_)
        {
            auto &min_list = freq_list_map_[min_freq_];
            const Key &evict_key = min_list.back();
            if (evictedKey)
            {
                *evictedKey = evict_key;
            }
            cache_map_.erase(evict_key);
            min_list.pop_back();
            if (min_list.empty())
            {
                freq_list_map_.erase(min_freq_);
            }
            stats_.evictions++;
        }

        min_freq_ = 1;
        freq_list_map_[1].push_front(key);
        cache_map_[key] = {value, 1, freq_list_map_[1].begin()};
        return true;
    }

    std::optional<Value> get(const Key &key) override
    {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end())
        {
            stats_.hits++;
            promote(key, it->second);
            return it->second.value;
        }
        stats_.misses++;
        return std::nullopt;
    }

    bool remove(const Key &key) override
    {
        auto it = cache_map_.find(key);
        if (it == cache_map_.end())
        {
            return false;
        }

        size_t freq = it->second.freq;
        freq_list_map_[freq].erase(it->second.it);
        if (freq_list_map_[freq].empty())
        {
            freq_list_map_.erase(freq);
            if (min_freq_ == freq && cache_map_.size() > 1)
            {
                min_freq_ = std::numeric_limits<size_t>::max();
                for (const auto &pair : freq_list_map_)
                {
                    if (pair.first < min_freq_)
                    {
                        min_freq_ = pair.first;
                    }
                }
            }
            else if (cache_map_.size() == 1)
            {
                min_freq_ = 0;
            }
        }
        cache_map_.erase(it);
        return true;
    }

    void clear() override
    {
        cache_map_.clear();
        freq_list_map_.clear();
        min_freq_ = 0;
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

    std::vector<std::pair<Key, size_t>> getKeysWithFrequency() const
    {
        std::vector<std::pair<Key, size_t>> result;
        result.reserve(cache_map_.size());
        for (const auto &pair : cache_map_)
        {
            result.emplace_back(pair.first, pair.second.freq);
        }
        return result;
    }

    std::vector<std::pair<Key, Value>> getAllEntries() const override
    {
        std::vector<std::pair<Key, Value>> entries;
        entries.reserve(cache_map_.size());
        for (const auto &pair : cache_map_)
        {
            entries.emplace_back(pair.first, pair.second.value);
        }
        return entries;
    }

    void setStats(const CacheStats &stats) override
    {
        stats_ = stats;
    }
};