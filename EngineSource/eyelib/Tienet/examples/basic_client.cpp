/**
 * @file basic_client.cpp
 * @brief Basic client example using Tienet
 */

#include <tienet.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

using namespace tienet;

int main() {
    std::cout << "🚀 Tienet Basic Client Example" << std::endl;
    
    // Initialize Tienet
    if (!tienet::initialize()) {
        std::cerr << "❌ Failed to initialize Tienet" << std::endl;
        return -1;
    }
    
    // Create UDP client
    client::UDPClient client;
    
    // Configure client
    client::ClientConfig config;
    config.serverAddress = "127.0.0.1";
    config.serverPort = 8080;
    config.autoReconnect = true;
    config.enableHeartbeat = true;
    
    // Set event callbacks
    client.setEventCallback([](client::ClientEvent event, const core::Packet& packet) {
        switch (event) {
            case client::ClientEvent::Connected:
                std::cout << "✅ Connected to server" << std::endl;
                break;
            case client::ClientEvent::Disconnected:
                std::cout << "❌ Disconnected from server" << std::endl;
                break;
            case client::ClientEvent::Error:
                std::cout << "⚠️ Client error" << std::endl;
                break;
            default:
                break;
        }
    });
    
    client.setDataCallback([](const core::Packet& packet) {
        std::cout << "📨 Received from server: " 
                  << std::string(reinterpret_cast<const char*>(packet.getData().data()), packet.getDataSize()) << std::endl;
    });
    
    // Initialize and connect
    if (!client.initialize(config)) {
        std::cerr << "❌ Failed to initialize client" << std::endl;
        return -1;
    }
    
    std::cout << "🔗 Connecting to " << config.serverAddress << ":" << config.serverPort << "..." << std::endl;
    
    if (!client.connect(config.serverAddress, config.serverPort)) {
        std::cerr << "❌ Failed to connect to server" << std::endl;
        return -1;
    }
    
    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    if (!client.isConnected()) {
        std::cerr << "❌ Not connected to server" << std::endl;
        return -1;
    }
    
    std::cout << "✅ Connected successfully!" << std::endl;
    std::cout << "💡 Type messages to send to server (type 'quit' to exit)" << std::endl;
    
    // Main loop
    bool running = true;
    while (running) {
        client.update();
        
        // Check for user input
        if (std::cin.peek() != std::char_traits<char>::eof()) {
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                running = false;
            } else if (input == "ping") {
                client.sendMessage("ping");
                std::cout << "📤 Sent ping" << std::endl;
            } else if (input == "stats") {
                auto stats = client.getStats();
                std::cout << "📊 Client Statistics:" << std::endl;
                std::cout << "  - Packets sent: " << stats.packetsSent << std::endl;
                std::cout << "  - Packets received: " << stats.packetsReceived << std::endl;
                std::cout << "  - Bytes sent: " << stats.bytesSent << std::endl;
                std::cout << "  - Bytes received: " << stats.bytesReceived << std::endl;
                std::cout << "  - Connection time: " << stats.connectionTime << "ms" << std::endl;
                std::cout << "  - Last latency: " << stats.lastLatency << "ms" << std::endl;
                std::cout << "  - Average latency: " << stats.averageLatency << "ms" << std::endl;
            } else if (!input.empty()) {
                client.sendMessage(input);
                std::cout << "📤 Sent: " << input << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Shutdown
    std::cout << "🛑 Disconnecting..." << std::endl;
    client.disconnect();
    client.shutdown();
    
    tienet::shutdown();
    std::cout << "✅ Client shutdown complete" << std::endl;
    
    return 0;
}
