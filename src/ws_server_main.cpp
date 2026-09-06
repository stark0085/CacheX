#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include "../server/event_broadcaster.hpp"
#include <iostream>

int main() {
    ix::initNetSystem();

    int port = 8081;
    ix::WebSocketServer server(port, "0.0.0.0");
    EventBroadcaster broadcaster;

    server.setOnConnectionCallback(
        [&broadcaster](std::weak_ptr<ix::WebSocket> webSocket,
                        std::shared_ptr<ix::ConnectionState> connectionState) {
            auto ws = webSocket.lock();
            if (!ws) return;

            std::cout << "New WS connection (id: " << connectionState->getId() << ")\n";
            broadcaster.addClient(webSocket);

            ws->setOnMessageCallback([&broadcaster](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open) {
                    std::cout << "Connection opened\n";
                } else if (msg->type == ix::WebSocketMessageType::Close) {
                    std::cout << "Connection closed\n";
                    broadcaster.removeExpired();
                } else if (msg->type == ix::WebSocketMessageType::Message) {
                    std::cout << "Received: " << msg->str << "\n";
                }
            });
        }
    );

    auto res = server.listen();
    if (!res.first) {
        std::cerr << "Failed to listen: " << res.second << "\n";
        return 1;
    }
    server.start();
    std::cout << "WebSocket server listening on ws://localhost:" << port << "\n";

    // Temporary smoke test: broadcast a fake event every 3 seconds so you can
    // confirm the broadcaster actually pushes to connected clients before we
    // wire it to the real cache.
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        broadcaster.broadcast("test", "demo-key", "LRU");
        std::cout << "Broadcasted test event to " << broadcaster.clientCount() << " client(s)\n";
    }

    return 0;
}