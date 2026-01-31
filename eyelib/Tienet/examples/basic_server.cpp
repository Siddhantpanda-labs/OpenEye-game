/**
 * @file basic_server.cpp
 * @brief Basic server example using Tienet
 */

#include <tienet.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace tienet;

int main() {
    std::cout << "🚀 Tienet Basic Server Example" << std::endl;
    
    // Initialize Tienet
    if (!tienet::initialize()) {
        std::cerr << "❌ Failed to initialize Tienet" << std::endl;
        return -1;
    }
    
    // Create UDP server
    server::UDPServer server;
    
    // Configure server
    server::ServerConfig config;
    config.port = 8080;
    config.maxClients = 10;
    config.bindAddress = "0.0.0.0";
    
    // Set event callbacks
    server.setEventCallback([](server::ServerEvent event, const server::ClientInfo& client, const core::Packet& packet) {
        switch (event) {
            case server::ServerEvent::ClientConnected:
                std::cout << "✅ Client connected: " << client.address.toString() << " (ID: " << client.id << ")" << std::endl;
                break;
            case server::ServerEvent::ClientDisconnected:
                std::cout << "❌ Client disconnected: " << client.address.toString() << " (ID: " << client.id << ")" << std::endl;
                break;
            case server::ServerEvent::Error:
                std::cout << "⚠️ Server error" << std::endl;
                break;
            default:
                break;
        }
    });
    
    server.setDataCallback([&](const server::ClientInfo& client, const core::Packet& packet) {
        std::cout << "📨 Received from " << client.address.toString() << ": " 
                  << std::string(reinterpret_cast<const char*>(packet.getData().data()), packet.getDataSize()) << std::endl;
        
        // Echo back to client
        std::string response = "Echo: " + std::string(reinterpret_cast<const char*>(packet.getData().data()), packet.getDataSize());
        server.sendToClient(client.id, response.c_str(), response.length());
    });
    
    // Initialize and start server
    if (!server.initialize(config)) {
        std::cerr << "❌ Failed to initialize server" << std::endl;
        return -1;
    }
    
    if (!server.start()) {
        std::cerr << "❌ Failed to start server" << std::endl;
        return -1;
    }
    
    std::cout << "✅ Server started on port " << config.port << std::endl;
    std::cout << "📡 Waiting for clients..." << std::endl;
    
    // Main loop
    bool running = true;
    while (running) {
        server.update();
        
        // Check for user input
        if (std::cin.peek() != std::char_traits<char>::eof()) {
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                running = false;
            } else if (input == "clients") {
                auto clients = server.getClients();
                std::cout << "📊 Connected clients: " << clients.size() << std::endl;
                for (const auto& client : clients) {
                    std::cout << "  - " << client.address.toString() << " (ID: " << client.id << ")" << std::endl;
                }
            } else if (input == "stats") {
                auto stats = server.getStats();
                std::cout << "📊 Server Statistics:" << std::endl;
                std::cout << "  - Total clients: " << stats.totalClients << std::endl;
                std::cout << "  - Active clients: " << stats.activeClients << std::endl;
                std::cout << "  - Packets sent: " << stats.totalPacketsSent << std::endl;
                std::cout << "  - Packets received: " << stats.totalPacketsReceived << std::endl;
                std::cout << "  - Bytes sent: " << stats.totalBytesSent << std::endl;
                std::cout << "  - Bytes received: " << stats.totalBytesReceived << std::endl;
            } else if (input.substr(0, 6) == "broadcast ") {
                std::string message = input.substr(9);
                uint32_t sent = server.broadcast(message.c_str(), message.length());
                std::cout << "📢 Broadcast sent to " << sent << " clients" << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Shutdown
    std::cout << "🛑 Shutting down server..." << std::endl;
    server.stop();
    server.shutdown();
    
    tienet::shutdown();
    std::cout << "✅ Server shutdown complete" << std::endl;
    
    return 0;
}
