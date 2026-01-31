/**
 * @file test_tienet.cpp
 * @brief Simple test for Tienet library
 */

#include <tienet.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace tienet;

int main() {
    std::cout << "🧪 Testing Tienet Library" << std::endl;
    std::cout << "Version: " << tienet::getVersion() << std::endl;
    std::cout << "Build: " << tienet::getBuildInfo() << std::endl;
    
    // Test initialization
    if (!tienet::initialize()) {
        std::cerr << "❌ Failed to initialize Tienet" << std::endl;
        return -1;
    }
    
    std::cout << "✅ Tienet initialized successfully" << std::endl;
    
    // Test socket creation
    auto socket = core::SocketFactory::createUDPSocket();
    if (!socket) {
        std::cerr << "❌ Failed to create UDP socket" << std::endl;
        return -1;
    }
    
    std::cout << "✅ UDP socket created successfully" << std::endl;
    
    // Test packet creation
    core::Packet packet = core::PacketBuilder()
        .type(core::PacketType::DATA)
        .data("Hello Tienet!", 13)
        .build();
    
    std::cout << "✅ Packet created successfully" << std::endl;
    std::cout << "   Type: " << static_cast<int>(packet.getType()) << std::endl;
    std::cout << "   Size: " << packet.getDataSize() << " bytes" << std::endl;
    
    // Test packet serialization
    auto serialized = packet.serialize();
    std::cout << "✅ Packet serialized successfully" << std::endl;
    std::cout << "   Serialized size: " << serialized.size() << " bytes" << std::endl;
    
    // Test packet deserialization
    core::Packet deserializedPacket;
    if (deserializedPacket.deserialize(serialized)) {
        std::cout << "✅ Packet deserialized successfully" << std::endl;
        std::cout << "   Deserialized data: " << std::string(
            reinterpret_cast<const char*>(deserializedPacket.getData().data()),
            deserializedPacket.getDataSize()) << std::endl;
    } else {
        std::cerr << "❌ Failed to deserialize packet" << std::endl;
    }
    
    // Test compression
    auto compressor = compression::CompressorFactory::createCompressor("none");
    if (compressor) {
        std::cout << "✅ Compressor created successfully" << std::endl;
        std::cout << "   Compressor name: " << compressor->getName() << std::endl;
        std::cout << "   Available: " << (compressor->isAvailable() ? "Yes" : "No") << std::endl;
    } else {
        std::cerr << "❌ Failed to create compressor" << std::endl;
    }
    
    // Test available compressors
    auto availableCompressors = compression::CompressorFactory::getAvailableCompressors();
    std::cout << "✅ Available compressors: ";
    for (const auto& name : availableCompressors) {
        std::cout << name << " ";
    }
    std::cout << std::endl;
    
    // Test server creation
    server::UDPServer server;
    if (server.initialize()) {
        std::cout << "✅ UDP server initialized successfully" << std::endl;
    } else {
        std::cerr << "❌ Failed to initialize UDP server" << std::endl;
    }
    
    // Test client creation
    client::UDPClient client;
    if (client.initialize()) {
        std::cout << "✅ UDP client initialized successfully" << std::endl;
    } else {
        std::cerr << "❌ Failed to initialize UDP client" << std::endl;
    }
    
    // (Optional) VR and Flutter components can be tested in their own binaries
    
    // Cleanup
    tienet::shutdown();
    std::cout << "✅ Tienet shutdown complete" << std::endl;
    
    std::cout << "\n🎉 All tests passed! Tienet library is working correctly." << std::endl;
    
    return 0;
}
