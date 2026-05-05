/**
 * @file vr_server.cpp
 * @brief VR server example using Tienet VR extensions
 */

#include <tienet.hpp>
#include <tienet/vr/vr_extensions.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

using namespace tienet;

int main() {
    std::cout << "🚀 Tienet VR Server Example" << std::endl;
    
    // Initialize Tienet
    if (!tienet::initialize()) {
        std::cerr << "❌ Failed to initialize Tienet" << std::endl;
        return -1;
    }
    
    // Create VR server
    tienet::vr::VRServer server;
    
    // Configure VR server
    tienet::vr::VRServerConfig config;
    config.port = 27016;
    config.maxClients = 5;
    config.frameRate = 60;
    config.quality = 80;
    config.enableCompression = true;
    config.compressionType = "lz4";
    
    // Set event callbacks
    server.setEventCallback([](server::ServerEvent event, const server::ClientInfo& client, const core::Packet& packet) {
        switch (event) {
            case server::ServerEvent::ClientConnected:
                std::cout << "✅ VR Client connected: " << client.address.toString() << " (ID: " << client.id << ")" << std::endl;
                break;
            case server::ServerEvent::ClientDisconnected:
                std::cout << "❌ VR Client disconnected: " << client.address.toString() << " (ID: " << client.id << ")" << std::endl;
                break;
            case server::ServerEvent::Error:
                std::cout << "⚠️ VR Server error" << std::endl;
                break;
            default:
                break;
        }
    });
    
    // Set VR-specific callbacks
    server.setGyroscopeCallback([](uint32_t clientId, const tienet::vr::GyroscopeData& data) {
        std::cout << "📱 Gyroscope data from client " << clientId << ": "
                  << "x=" << data.x << ", y=" << data.y << ", z=" << data.z << std::endl;
    });
    
    server.setTouchCallback([](uint32_t clientId, const tienet::vr::TouchData& data) {
        std::cout << "👆 Touch data from client " << clientId << ": "
                  << "x=" << data.x << ", y=" << data.y << ", pressed=" << data.isPressed << std::endl;
    });
    
    // Initialize and start server
    if (!server.initialize(config)) {
        std::cerr << "❌ Failed to initialize VR server" << std::endl;
        return -1;
    }
    
    if (!server.start()) {
        std::cerr << "❌ Failed to start VR server" << std::endl;
        return -1;
    }
    
    std::cout << "✅ VR Server started on port " << config.port << std::endl;
    std::cout << "📡 Waiting for VR clients..." << std::endl;
    std::cout << "💡 Commands: 'frame' (send test frame), 'clients', 'stats', 'quit'" << std::endl;
    
    // Random number generator for test data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    // Main loop
    bool running = true;
    uint32_t frameCounter = 0;
    
    while (running) {
        server.update();
        
        // Check for user input
        if (std::cin.peek() != std::char_traits<char>::eof()) {
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                running = false;
            } else if (input == "frame") {
                // Create a test VR frame
                tienet::vr::VRFrame frame(1280, 720, 0); // RGBA format
                frame.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
                
                // Generate random pixel data
                frame.data.resize(1280 * 720 * 4); // RGBA
                for (size_t i = 0; i < frame.data.size(); i += 4) {
                    frame.data[i] = dis(gen);     // R
                    frame.data[i + 1] = dis(gen); // G
                    frame.data[i + 2] = dis(gen); // B
                    frame.data[i + 3] = 255;      // A
                }
                
                // Send frame to all clients
                uint32_t sent = server.sendFrame(frame);
                std::cout << "📤 Sent test frame " << frameCounter++ << " to " << sent << " clients" << std::endl;
            } else if (input == "clients") {
                auto clients = server.getClients();
                std::cout << "📊 Connected VR clients: " << clients.size() << std::endl;
                for (const auto& client : clients) {
                    std::cout << "  - " << client.address.toString() << " (ID: " << client.id << ")" << std::endl;
                }
            } else if (input == "stats") {
                auto stats = server.getStats();
                std::cout << "📊 VR Server Statistics:" << std::endl;
                std::cout << "  - Total clients: " << stats.totalClients << std::endl;
                std::cout << "  - Active clients: " << stats.activeClients << std::endl;
                std::cout << "  - Packets sent: " << stats.totalPacketsSent << std::endl;
                std::cout << "  - Packets received: " << stats.totalPacketsReceived << std::endl;
                std::cout << "  - Bytes sent: " << stats.totalBytesSent << std::endl;
                std::cout << "  - Bytes received: " << stats.totalBytesReceived << std::endl;
                std::cout << "  - Compression: " << (server.isCompressionEnabled() ? "Enabled" : "Disabled") << std::endl;
                std::cout << "  - Compression type: " << server.getCompressionType() << std::endl;
            } else if (input == "compression") {
                bool enabled = server.isCompressionEnabled();
                server.setCompressionEnabled(!enabled);
                std::cout << "🔄 Compression " << (!enabled ? "enabled" : "disabled") << std::endl;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Shutdown
    std::cout << "🛑 Shutting down VR server..." << std::endl;
    server.stop();
    server.shutdown();
    
    tienet::shutdown();
    std::cout << "✅ VR Server shutdown complete" << std::endl;
    
    return 0;
}
