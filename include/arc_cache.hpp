#pragma once

#include "cache_interface.hpp"
#include <unordered_map>
#include <list>
#include <vector>
#include <stdexcept>
#include <cassert>
#include <algorithm>

template <typename Key, typename Value>
class ARCCache : public ICache<Key, Value> {
private:
    enum class ListType { T1, T2, B1, B2 };

    struct Location {
        ListType list;
        typename std::list<Key>::iterator it;
    };

    size_t capacity_;
    double p_;
    CacheStats stats_;

    std::list<Key> T1_, T2_, B1_, B2_;
    std::unordered_map<Key, Location> location_map_;
    std::unordered_map<Key, Value>   value_map_;

    // Evict one live entry: move LRU of T1->B1 or LRU of T2->B2.
    // came_from_b2: true when the incoming put() key originated from a B2 ghost hit.
    void replace(const Key& incoming_key, bool came_from_b2) {
        bool prefer_t1 = !T1_.empty() &&
                         (T1_.size() > static_cast<size_t>(p_) ||
                          (came_from_b2 && T1_.size() == static_cast<size_t>(p_)));

        if (prefer_t1) {
            Key evicted = T1_.back();
            T1_.pop_back();
            value_map_.erase(evicted);
            location_map_.erase(evicted);
            B1_.push_front(evicted);
            location_map_[evicted] = {ListType::B1, B1_.begin()};
        } else {
            Key evicted = T2_.back();
            T2_.pop_back();
            value_map_.erase(evicted);
            location_map_.erase(evicted);
            B2_.push_front(evicted);
            location_map_[evicted] = {ListType::B2, B2_.begin()};
        }
        stats_.evictions++;
    }

    void insert_into_t2(const Key& key, const Value& value) {
        T2_.push_front(key);
        location_map_[key] = {ListType::T2, T2_.begin()};
        value_map_[key] = value;
    }

    void insert_into_t1(const Key& key, const Value& value) {
        T1_.push_front(key);
        location_map_[key] = {ListType::T1, T1_.begin()};
        value_map_[key] = value;
    }

    // Pop LRU of B1 and remove from location_map.
    void evict_b1_lru() {
        if (B1_.empty()) return;
        location_map_.erase(B1_.back());
        B1_.pop_back();
    }

    // Pop LRU of B2 and remove from location_map.
    void evict_b2_lru() {
        if (B2_.empty()) return;
        location_map_.erase(B2_.back());
        B2_.pop_back();
    }

    // Pop LRU of T1 directly (branch A sub-case |T1|==c), counting as an eviction.
    void evict_t1_lru_direct() {
        Key evicted = T1_.back();
        T1_.pop_back();
        value_map_.erase(evicted);
        location_map_.erase(evicted);
        stats_.evictions++;
    }

public:
    explicit ARCCache(size_t capacity) : capacity_(capacity), p_(0.0) {
        if (capacity == 0)
            throw std::invalid_argument("Capacity must be greater than 0");
    }

    bool put(const Key& key, const Value& value) override {
        auto it = location_map_.find(key);

        if (it != location_map_.end()) {
            ListType where = it->second.list;

            // Case 1: cache hit — key is live in T1 or T2.
            if (where == ListType::T1 || where == ListType::T2) {
                stats_.hits++;
                if (where == ListType::T1)
                    T1_.erase(it->second.it);
                else
                    T2_.erase(it->second.it);
                location_map_.erase(key);
                value_map_.erase(key);
                insert_into_t2(key, value);
                return true;
            }

            // Case 2: ghost hit in B1 — adapt p upward.
            if (where == ListType::B1) {
                double delta = std::max(1.0, static_cast<double>(B2_.size()) /
                                             static_cast<double>(B1_.size()));
                p_ = std::min(p_ + delta, static_cast<double>(capacity_));

                B1_.erase(it->second.it);
                location_map_.erase(key);
                replace(key, /*came_from_b2=*/false);
                insert_into_t2(key, value);
                return true;
            }

            // Case 3: ghost hit in B2 — adapt p downward.
            // (where == ListType::B2)
            double delta = std::max(1.0, static_cast<double>(B1_.size()) /
                                         static_cast<double>(B2_.size()));
            p_ = std::max(p_ - delta, 0.0);

            B2_.erase(it->second.it);
            location_map_.erase(key);
            replace(key, /*came_from_b2=*/true);
            insert_into_t2(key, value);
            return true;
        }

        // Case 4: true miss — key is in none of the four lists.
        stats_.misses++;

        size_t l1    = T1_.size() + B1_.size();
        size_t total = T1_.size() + T2_.size() + B1_.size() + B2_.size();
        size_t live  = T1_.size() + T2_.size();

        if (l1 == capacity_) {
            // Branch A: the recency directory is full.
            if (T1_.size() < capacity_) {
                // Sub-case A1: trim ghost B1, then do a live eviction.
                evict_b1_lru();
                replace(key, /*came_from_b2=*/false);
            } else {
                // Sub-case A2: T1 takes all of c (B1 is empty).
                // Pop T1 LRU directly — this IS the eviction, no REPLACE needed.
                evict_t1_lru_direct();
            }
        } else if (total >= capacity_) {
            // Branch B: recency directory not full, but total directory is at/above c.
            // Assert: live cache must be exactly full here.
            // If this fires, REPLACE would be called unnecessarily — stop immediately.
#ifndef NDEBUG
            assert(live == capacity_ &&
                   "Branch B invariant violated: live cache is not full before REPLACE");
#endif
            if (total == 2 * capacity_) {
                evict_b2_lru();
            }
            replace(key, /*came_from_b2=*/false);
        }
        // else: cache/directory not full yet — just insert; no eviction needed.

        insert_into_t1(key, value);
        return true;
    }

    std::optional<Value> get(const Key& key) override {
        auto it = location_map_.find(key);
        if (it == location_map_.end() ||
            (it->second.list != ListType::T1 && it->second.list != ListType::T2)) {
            stats_.misses++;
            return std::nullopt;
        }

        // Hit: promote to MRU of T2.
        stats_.hits++;
        ListType where = it->second.list;
        Value val = value_map_.at(key);

        if (where == ListType::T1)
            T1_.erase(it->second.it);
        else
            T2_.erase(it->second.it);

        location_map_.erase(key);
        T2_.push_front(key);
        location_map_[key] = {ListType::T2, T2_.begin()};
        return val;
    }

    bool remove(const Key& key) override {
        auto it = location_map_.find(key);
        if (it == location_map_.end()) return false;

        ListType where = it->second.list;
        switch (where) {
            case ListType::T1: T1_.erase(it->second.it); value_map_.erase(key); break;
            case ListType::T2: T2_.erase(it->second.it); value_map_.erase(key); break;
            case ListType::B1: B1_.erase(it->second.it); break;
            case ListType::B2: B2_.erase(it->second.it); break;
        }
        location_map_.erase(it);
        return true;
    }

    void clear() override {
        T1_.clear(); T2_.clear(); B1_.clear(); B2_.clear();
        location_map_.clear();
        value_map_.clear();
        p_ = 0.0;
    }

    size_t size()     const override { return T1_.size() + T2_.size(); }
    size_t capacity() const override { return capacity_; }

    CacheStats getStats()   const override { return stats_; }
    void       resetStats()       override { stats_ = CacheStats{}; }

    // Debug state for the future visualizer.
    struct ARCDebugState {
        std::vector<Key> t1, t2, b1, b2;
        double p;
    };

    ARCDebugState getDebugState() const {
        ARCDebugState s;
        s.t1 = std::vector<Key>(T1_.begin(), T1_.end());
        s.t2 = std::vector<Key>(T2_.begin(), T2_.end());
        s.b1 = std::vector<Key>(B1_.begin(), B1_.end());
        s.b2 = std::vector<Key>(B2_.begin(), B2_.end());
        s.p  = p_;
        return s;
    }
};
