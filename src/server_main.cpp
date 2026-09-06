#include "httplib.h"
#include "../include/lru_cache.hpp"
#include "../include/lfu_cache.hpp"
#include "../server/event_broadcaster.hpp"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

int main()
{
    ix::initNetSystem();

    constexpr size_t kCacheCapacity = 100;

    // Hold the cache via the ICache interface so we can swap concrete
    // implementations (LRU <-> LFU) at runtime. Guarded by cache_mutex,
    // same as every individual cache operation.
    std::unique_ptr<ICache<std::string, std::string>> cache =
        std::make_unique<LRUCache<std::string, std::string>>(kCacheCapacity);
    std::mutex cache_mutex;
    std::atomic<bool> current_policy_is_lru{true}; // for reporting in events/stats

    EventBroadcaster broadcaster;

    auto policyName = [&]()
    {
        return current_policy_is_lru.load() ? std::string("LRU") : std::string("LFU");
    };

    httplib::Server rest_server;

    // CORS: allow any origin to call this API. Fine for a local dev/demo
    // server; a real production service would restrict this to known origins.
    rest_server.set_default_headers({{"Access-Control-Allow-Origin", "*"},
                                     {"Access-Control-Allow-Methods", "GET, PUT, DELETE, POST, OPTIONS"},
                                     {"Access-Control-Allow-Headers", "Content-Type"}});

    // Browsers send a pre-flight OPTIONS request before PUT/DELETE from a
    // different origin; respond 200 to every OPTIONS so pre-flight succeeds.
    rest_server.Options(R"(.*)", [](const httplib::Request &, httplib::Response &res)
                        { res.status = 200; });

    rest_server.Get(R"(/cache/([^/]+))", [&](const httplib::Request &req, httplib::Response &res)
                    {
        std::string key = req.matches[1];
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto val = cache->get(key);
        if (val.has_value()) {
            res.set_content(*val, "text/plain");
            res.status = 200;
            broadcaster.broadcast("hit", key, policyName());
        } else {
            res.status = 404;
            res.set_content("Not found", "text/plain");
            broadcaster.broadcast("miss", key, policyName());
        } });

    rest_server.Put(R"(/cache/([^/]+))", [&](const httplib::Request &req, httplib::Response &res)
                    {
        std::string key = req.matches[1];
        std::lock_guard<std::mutex> lock(cache_mutex);

        std::string evictedKey;
        cache->put(key, req.body, &evictedKey);
        bool hadEvictedKey = !evictedKey.empty() && evictedKey != key;

        broadcaster.broadcast("set", key, policyName());
        if (hadEvictedKey) {
            broadcaster.broadcast("eviction", evictedKey, policyName());
        }
        res.status = 204; });

    rest_server.Delete(R"(/cache/([^/]+))", [&](const httplib::Request &req, httplib::Response &res)
                       {
        std::string key = req.matches[1];
        std::lock_guard<std::mutex> lock(cache_mutex);
        bool removed = cache->remove(key);
        res.status = removed ? 204 : 404;
        if (removed) broadcaster.broadcast("remove", key, policyName()); });

    rest_server.Get("/stats", [&](const httplib::Request &, httplib::Response &res)
                    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto stats = cache->getStats();
        std::string json = "{\"policy\":\"" + policyName() + "\","
                            "\"hits\":" + std::to_string(stats.hits) +
                            ",\"misses\":" + std::to_string(stats.misses) +
                            ",\"evictions\":" + std::to_string(stats.evictions) +
                            ",\"hitRate\":" + std::to_string(stats.hitRate()) + "}";
        res.set_content(json, "application/json"); });

    // POST /policy  body: "LRU" or "LFU"
    // Switches the active eviction policy. As a deliberate design choice,
    // this creates a brand-new, empty cache under the new policy rather than
    // migrating existing entries — migrating wouldn't meaningfully preserve
    // recency/frequency history anyway, since a key's "hotness" under one
    // policy has no honest translation into the other policy's bookkeeping.
    rest_server.Post("/policy", [&](const httplib::Request &req, httplib::Response &res)
                     {
        std::string requested = req.body;
        // Trim potential whitespace/newlines from curl -d input.
        while (!requested.empty() && (requested.back() == '\n' || requested.back() == '\r' || requested.back() == ' ')) {
            requested.pop_back();
        }

        std::lock_guard<std::mutex> lock(cache_mutex);

        if (requested == "LRU") {
            cache = std::make_unique<LRUCache<std::string, std::string>>(kCacheCapacity);
            current_policy_is_lru = true;
        } else if (requested == "LFU") {
            cache = std::make_unique<LFUCache<std::string, std::string>>(kCacheCapacity);
            current_policy_is_lru = false;
        } else {
            res.status = 400;
            res.set_content("Invalid policy. Use 'LRU' or 'LFU'.", "text/plain");
            return;
        }

        broadcaster.broadcast("policy_change", "(none)", policyName());
        res.status = 200;
        res.set_content("Switched to " + policyName() + ". Cache has been reset.", "text/plain"); });

    std::thread rest_thread([&rest_server]()
                            {
        std::cout << "REST server listening on http://localhost:8080\n";
        rest_server.listen("0.0.0.0", 8080); });

    ix::WebSocketServer ws_server(8081, "0.0.0.0");

    ws_server.setOnConnectionCallback(
        [&broadcaster](std::weak_ptr<ix::WebSocket> webSocket,
                       std::shared_ptr<ix::ConnectionState> connectionState)
        {
            auto ws = webSocket.lock();
            if (!ws)
                return;

            std::cout << "New WS connection (id: " << connectionState->getId() << ")\n";
            broadcaster.addClient(webSocket);

            ws->setOnMessageCallback([&broadcaster](const ix::WebSocketMessagePtr &msg)
                                     {
                if (msg->type == ix::WebSocketMessageType::Close) {
                    broadcaster.removeExpired();
                } });
        });

    auto res = ws_server.listen();
    if (!res.first)
    {
        std::cerr << "WS server failed to listen: " << res.second << "\n";
        rest_server.stop(); // Ask the REST server to shut down cleanly...
        rest_thread.join(); // ...and wait for its thread to actually finish
                            // before main() returns, so ~thread() doesn't
                            // call std::terminate() on a still-running thread.
        return 1;
    }
    ws_server.start();
    std::cout << "WebSocket server listening on ws://localhost:8081\n";

    rest_thread.join();
    return 0;
}