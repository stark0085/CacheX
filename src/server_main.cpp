#include "httplib.h"
#include "../include/lru_cache.hpp"
#include "../include/lfu_cache.hpp"
#include "../include/fifo_cache.hpp"
#include "../include/mru_cache.hpp"
#include "../include/ttl_cache_wrapper.hpp"
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

    std::mutex cache_mutex;

    // Tracks the currently active policy as a plain string, guarded by
    // cache_mutex (same as every cache operation). Replaces the old
    // bool current_policy_is_lru, which only scaled to two policies.
    std::string current_policy = "LRU";

    EventBroadcaster broadcaster;

    auto policyName = [&]()
    {
        return current_policy;
    };

    // Wraps any concrete ICache in TTL support, with the sweep thread
    // configured to broadcast an "expired" event whenever it (or a lazy
    // check during get()/put()) reclaims a key — this is what keeps the
    // frontend grid in sync even when nobody explicitly touches an
    // expired key. Defined AFTER broadcaster/policyName since the
    // callback captures both by reference.
    auto makeCache = [&](std::unique_ptr<ICache<std::string, std::string>> inner)
    {
        return std::make_unique<TTLCacheWrapper<std::string, std::string>>(
            std::move(inner),
            std::chrono::seconds(60),
            std::chrono::milliseconds(1000),
            [&broadcaster, &policyName](const std::string &key)
            {
                broadcaster.broadcast("expired", key, policyName());
            });
    };

    // Hold the cache via the ICache interface (through the TTL wrapper) so
    // we can swap concrete implementations (LRU/LFU/FIFO/MRU) at runtime.
    // Guarded by cache_mutex, same as every individual cache operation.
    std::unique_ptr<TTLCacheWrapper<std::string, std::string>> cache =
        makeCache(std::make_unique<LRUCache<std::string, std::string>>(kCacheCapacity));

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

        // Optional ?ttl=<seconds> query param overrides the wrapper's default.
        if (req.has_param("ttl")) {
            long ttlSeconds = std::atol(req.get_param_value("ttl").c_str());
            if (ttlSeconds <= 0) {
                res.status = 400;
                res.set_content("ttl must be a positive integer (seconds)", "text/plain");
                return;
            }
            cache->putWithTTL(key, req.body, std::chrono::seconds(ttlSeconds), &evictedKey);
        } else {
            cache->put(key, req.body, &evictedKey);
        }

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

    // DELETE /cache  (no key segment) — wipes every entry in the current cache.
    rest_server.Delete("/cache", [&](const httplib::Request &, httplib::Response &res)
                       {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache->clear();
        broadcaster.broadcast("clear", "(all)", policyName());
        res.status = 204; });

    // GET /cache-keys — returns every key currently in the cache as a JSON
    // array, e.g. ["foo","bar"]. Used by the frontend to resync its local
    // grid after a policy switch, since switching now migrates data rather
    // than wiping it.
    rest_server.Get("/cache-keys", [&](const httplib::Request &, httplib::Response &res)
                    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto entries = cache->getAllEntries();
        std::string json = "[";
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) json += ",";
            json += "\"" + entries[i].first + "\"";
        }
        json += "]";
        res.set_content(json, "application/json"); });

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

    // POST /policy  body: "LRU", "LFU", "FIFO", or "MRU"
    // Switches the active eviction policy. As a deliberate design choice,
    // this creates a brand-new, empty cache under the new policy rather than
    // migrating existing entries — migrating wouldn't meaningfully preserve
    // recency/frequency history anyway, since a key's "hotness" under one
    // policy has no honest translation into another policy's bookkeeping.
    // Uses makeCache() so the new instance also gets the on_expired callback
    // wired up — without this, switching policy would silently stop
    // broadcasting "expired" events.
    rest_server.Post("/policy", [&](const httplib::Request &req, httplib::Response &res)
                     {
        std::string requested = req.body;
        // Trim potential whitespace/newlines from curl -d input.
        while (!requested.empty() && (requested.back() == '\n' || requested.back() == '\r' || requested.back() == ' ')) {
            requested.pop_back();
        }

               std::lock_guard<std::mutex> lock(cache_mutex);

        if (requested != "LRU" && requested != "LFU" && requested != "FIFO" && requested != "MRU") {
            res.status = 400;
            res.set_content("Invalid policy. Use 'LRU', 'LFU', 'FIFO', or 'MRU'.", "text/plain");
            return;
        }

        // Migrate existing data and stats to the new policy, mirroring
        // Redis's CONFIG SET maxmemory-policy: changing eviction strategy
        // does not clear the dataset. We deliberately do NOT try to carry
        // over recency/frequency metadata (e.g. an LFU key's access count)
        // since it has no honest meaning under a different policy — only
        // the raw key-value pairs and aggregate stats are preserved.
        auto oldEntries = cache->getAllEntries();
        auto oldStats = cache->getStats();

        if (requested == "LRU") {
            cache = makeCache(std::make_unique<LRUCache<std::string, std::string>>(kCacheCapacity));
        } else if (requested == "LFU") {
            cache = makeCache(std::make_unique<LFUCache<std::string, std::string>>(kCacheCapacity));
        } else if (requested == "FIFO") {
            cache = makeCache(std::make_unique<FIFOCache<std::string, std::string>>(kCacheCapacity));
        } else { // MRU
            cache = makeCache(std::make_unique<MRUCache<std::string, std::string>>(kCacheCapacity));
        }

        for (const auto& entry : oldEntries) {
            cache->put(entry.first, entry.second);
        }
        cache->setStats(oldStats);

        current_policy = requested;
        broadcaster.broadcast("policy_change", "(none)", policyName());
        res.status = 200;
        res.set_content("Switched to " + policyName() + ". " +
                         std::to_string(oldEntries.size()) + " entries migrated.", "text/plain"); });

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