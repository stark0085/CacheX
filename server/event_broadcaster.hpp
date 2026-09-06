#pragma once

#include <ixwebsocket/IXWebSocket.h>
#include <set>
#include <memory>
#include <mutex>
#include <string>
#include <chrono>
#include <sstream>

/**
 * @brief Tracks connected WebSocket clients and broadcasts JSON cache events to all of them.
 * Thread-safe: cache operations happen on the REST server's threads, while
 * connection add/remove happens on IXWebSocket's own thread(s).
 */
class EventBroadcaster {
public:
    void addClient(std::weak_ptr<ix::WebSocket> client) {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.push_back(client);
    }

    // Called on every message/open/close event so we can prune dead connections
    // opportunistically rather than needing a separate cleanup pass.
    void removeExpired() {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                [](const std::weak_ptr<ix::WebSocket>& wp) { return wp.expired(); }),
            clients_.end()
        );
    }

    /**
     * @brief Send a cache event to every connected client.
     * @param type   "hit" | "miss" | "eviction" | "set" | "remove"
     * @param key    The cache key involved.
     * @param policy Current eviction policy name, e.g. "LRU" or "LFU".
     */
    void broadcast(const std::string& type, const std::string& key, const std::string& policy) {
        std::string json = buildEventJson(type, key, policy);

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& weakClient : clients_) {
            if (auto client = weakClient.lock()) {
                client->send(json);
            }
            // expired weak_ptrs are left for removeExpired() to clean up;
            // we don't erase mid-iteration here to keep this loop simple and safe.
        }
    }

    size_t clientCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (auto& wp : clients_) {
            if (!wp.expired()) ++count;
        }
        return count;
    }

private:
    std::string buildEventJson(const std::string& type, const std::string& key, const std::string& policy) const {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

        std::ostringstream oss;
        oss << "{"
            << "\"type\":\"" << escapeJson(type) << "\","
            << "\"key\":\"" << escapeJson(key) << "\","
            << "\"policy\":\"" << escapeJson(policy) << "\","
            << "\"timestamp\":" << ms
            << "}";
        return oss.str();
    }

    // Minimal escaping for quotes/backslashes only — not a general JSON escaper,
    // sufficient for our controlled event/key/policy strings.
    static std::string escapeJson(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    }

    mutable std::mutex mutex_;
    std::vector<std::weak_ptr<ix::WebSocket>> clients_;
};