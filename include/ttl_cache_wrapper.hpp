#pragma once

#include "cache_interface.hpp"
#include <memory>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

/**
 * @brief Decorator that adds TTL (time-to-live) expiry on top of any ICache.
 *
 * Uses BOTH lazy expiry (checked on every get()/put()) and an active
 * background sweep thread that periodically reclaims expired entries,
 * so capacity isn't wasted holding dead entries indefinitely between
 * accesses.
 *
 * Thread-safe: all public methods and the sweep thread share one mutex.
 */
template <typename Key, typename Value>
class TTLCacheWrapper : public ICache<Key, Value>
{
public:
    /**
     * @param inner_cache The cache to wrap (LRU, LFU, etc.)
     * @param default_ttl TTL applied to every put() unless overridden.
     * @param sweep_interval How often the background thread checks for
     *                        expired entries. Shorter = more prompt
     *                        reclamation, more background CPU wakeups.
     */
    using ExpiryCallback = std::function<void(const Key &)>;

    TTLCacheWrapper(std::unique_ptr<ICache<Key, Value>> inner_cache,
                    std::chrono::seconds default_ttl,
                    std::chrono::milliseconds sweep_interval = std::chrono::milliseconds(1000),
                    ExpiryCallback on_expired = nullptr)
        : inner_cache_(std::move(inner_cache)),
          default_ttl_(default_ttl),
          sweep_interval_(sweep_interval),
          on_expired_(std::move(on_expired)),
          stop_requested_(false)
    {
        sweep_thread_ = std::thread(&TTLCacheWrapper::sweepLoop, this);
    }

    ~TTLCacheWrapper() override
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_requested_ = true;
        }
        cv_.notify_all();
        if (sweep_thread_.joinable())
        {
            sweep_thread_.join();
        }
    }

    // Non-copyable (owns a thread and a unique_ptr); movable is also
    // disabled for simplicity since nothing in this project needs to
    // move a TTLCacheWrapper after construction.
    TTLCacheWrapper(const TTLCacheWrapper &) = delete;
    TTLCacheWrapper &operator=(const TTLCacheWrapper &) = delete;

    bool put(const Key &key, const Value &value, Key *evictedKey = nullptr) override
    {
        return putWithTTL(key, value, default_ttl_, evictedKey);
    }

    /**
     * @brief Insert with an explicit per-key TTL, overriding the wrapper's default.
     * Not part of the ICache interface (interface stays TTL-agnostic by
     * design), but callable directly on TTLCacheWrapper when a caller
     * (like the server layer) needs per-key control.
     */
    bool putWithTTL(const Key &key, const Value &value,
                    std::chrono::seconds ttl, Key *evictedKey = nullptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        removeIfExpired(key);

        bool result = inner_cache_->put(key, value, evictedKey);
        expiry_map_[key] = std::chrono::steady_clock::now() + ttl;

        if (evictedKey && !evictedKey->empty())
        {
            expiry_map_.erase(*evictedKey);
        }
        return result;
    }

    std::optional<Value> get(const Key &key) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (removeIfExpired(key))
        {
            return std::nullopt; // treat as a miss; don't touch inner_cache_
                                 // at all so we don't pollute its recency/
                                 // frequency bookkeeping with a dead key.
        }
        return inner_cache_->get(key);
    }

    bool remove(const Key &key) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        expiry_map_.erase(key);
        return inner_cache_->remove(key);
    }

    void clear() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        expiry_map_.clear();
        inner_cache_->clear();
    }

    size_t size() const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return inner_cache_->size();
        // Note: this may include entries that are logically expired but
        // not yet swept — an honest limitation of combining lazy + active
        // expiry rather than fully synchronous accounting.
    }

    size_t capacity() const override
    {
        return inner_cache_->capacity();
    }

    CacheStats getStats() const override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return inner_cache_->getStats();
    }

    void resetStats() override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        inner_cache_->resetStats();
    }

private:
    // Caller must already hold mutex_. Returns true if the key was
    // expired (and has now been removed from both maps).
    bool removeIfExpired(const Key &key)
    {
        auto it = expiry_map_.find(key);
        if (it == expiry_map_.end())
        {
            return false;
        }
        if (std::chrono::steady_clock::now() >= it->second)
        {
            inner_cache_->remove(key);
            expiry_map_.erase(it);
            if (on_expired_)
                on_expired_(key); // notify caller, e.g. to broadcast a WS event
            return true;
        }
        return false;
    }

    void sweepLoop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!stop_requested_)
        {
            cv_.wait_for(lock, sweep_interval_, [this]
                         { return stop_requested_.load(); });
            if (stop_requested_)
                break;

            auto now = std::chrono::steady_clock::now();
            for (auto it = expiry_map_.begin(); it != expiry_map_.end();)
            {
                if (now >= it->second)
                {
                    Key expiredKey = it->first; // copy before erase invalidates the iterator
                    inner_cache_->remove(expiredKey);
                    it = expiry_map_.erase(it);
                    if (on_expired_)
                        on_expired_(expiredKey);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    std::unique_ptr<ICache<Key, Value>> inner_cache_;
    std::chrono::seconds default_ttl_;
    std::chrono::milliseconds sweep_interval_;
    ExpiryCallback on_expired_;
    std::unordered_map<Key, std::chrono::steady_clock::time_point> expiry_map_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread sweep_thread_;
    std::atomic<bool> stop_requested_;
};