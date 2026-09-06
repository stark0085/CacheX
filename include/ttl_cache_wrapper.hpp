#pragma once

#include "cache_interface.hpp"
#include <memory>
#include <chrono>

template <typename Key, typename Value>
class TTLCacheWrapper : public ICache<Key, Value>
{
public:
    explicit TTLCacheWrapper(std::unique_ptr<ICache<Key, Value>> inner_cache, std::chrono::seconds default_ttl)
        : inner_cache_(std::move(inner_cache)), default_ttl_(default_ttl) {}

    bool put(const Key &key, const Value &value, Key *evictedKey = nullptr) override
    {
        // TODO: Implement TTL logic (e.g., store expiry timestamp somewhere)
        return inner_cache_->put(key, value, evictedKey);
    }

    std::optional<Value> get(const Key &key) override
    {
        // TODO: Check if expired before returning
        return inner_cache_->get(key);
    }

    bool remove(const Key &key) override
    {
        // TODO: Forward and manage TTL cleanup
        return inner_cache_->remove(key);
    }

    void clear() override
    {
        // TODO: Clear internal TTL state
        inner_cache_->clear();
    }

    size_t size() const override
    {
        // TODO: Maybe exclude expired items from size?
        return inner_cache_->size();
    }

    size_t capacity() const override
    {
        return inner_cache_->capacity();
    }

    CacheStats getStats() const override
    {
        return inner_cache_->getStats();
    }

    void resetStats() override
    {
        inner_cache_->resetStats();
    }

private:
    std::unique_ptr<ICache<Key, Value>> inner_cache_;
    std::chrono::seconds default_ttl_;

    // TODO: Need a secondary map to store expiry timestamps for each key,
    // since the inner ICache<Key, Value> only stores Value.
};
