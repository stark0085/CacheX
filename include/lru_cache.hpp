#pragma once

#include "cache_interface.hpp"
#include <list>
#include <unordered_map>
#include <stdexcept>
#include <vector>

template <typename Key, typename Value>
class LRUCache : public ICache<Key, Value> {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("Capacity must be greater than 0");
        }
    }

    bool put(const Key& key, const Value& value, Key* evictedKey = nullptr) override {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Key already exists, update value and move to front
            it->second->second = value;
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
        } else {
            // New key
            if (cache_map_.size() >= capacity_) {
                // Evict least recently used (back of list)
                const auto& last = cache_list_.back();
                if (evictedKey) {
                    *evictedKey = last.first;
                }
                cache_map_.erase(last.first);
                cache_list_.pop_back();
                stats_.evictions++;
            }
            // Insert new entry at the front
            cache_list_.emplace_front(key, value);
            cache_map_[key] = cache_list_.begin();
        }
        return true;
    }

    std::optional<Value> get(const Key& key) override {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            stats_.hits++;
            // Move accessed item to the front
            cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
            return it->second->second;
        } else {
            stats_.misses++;
            return std::nullopt;
        }
    }

    bool remove(const Key& key) override {
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            cache_list_.erase(it->second);
            cache_map_.erase(it);
            return true;
        }
        return false;
    }

    void clear() override {
        cache_map_.clear();
        cache_list_.clear();
    }

    size_t size() const override {
        return cache_map_.size();
    }

    size_t capacity() const override {
        return capacity_;
    }

    CacheStats getStats() const override {
        return stats_;
    }

    void resetStats() override {
        stats_ = CacheStats{};
    }

    std::vector<Key> getKeysInOrder() const {
        std::vector<Key> keys;
        keys.reserve(cache_list_.size());
        for (const auto& pair : cache_list_) {
            keys.push_back(pair.first);
        }
        return keys;
    }

private:
    size_t capacity_;
    CacheStats stats_;
    std::list<std::pair<Key, Value>> cache_list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map_;
};