#pragma once

#include "cache_interface.hpp"

template <typename Key, typename Value>
class LFUCache : public CacheInterface<Key, Value> {
public:
    explicit LFUCache(size_t capacity) : capacity_(capacity) {}

    void put(const Key& key, const Value& value) override {
        (void)key; (void)value; // stub
    }

    std::optional<Value> get(const Key& key) override {
        (void)key;
        return std::nullopt; // stub
    }

    size_t size() const override {
        return 0; // stub
    }

    void clear() override {
        // stub
    }

private:
    size_t capacity_;
};
