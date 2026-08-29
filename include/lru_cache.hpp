#pragma once

#include "cache_interface.hpp"

template <typename Key, typename Value>
class LRUCache : public ICache<Key, Value> {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    bool put(const Key& key, const Value& value) override {
        (void)key; (void)value; // stub
        return true;
    }

    std::optional<Value> get(const Key& key) override {
        (void)key;
        return std::nullopt; // stub
    }

    bool remove(const Key& key) override {
        (void)key;
        return false; // stub
    }

    void clear() override {
        // stub
    }

    size_t size() const override {
        return 0; // stub
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

private:
    size_t capacity_;
    CacheStats stats_;
};
