/**
 * @file flutter_bindings.cpp
 * @brief Flutter platform channel bindings implementation
 */

#include "tienet/flutter/flutter_bindings.hpp"
#include "tienet.hpp"
#include "tienet/vr/vr_extensions.hpp"
#include <iostream>
#include <sstream>

namespace tienet {
namespace flutter {

// FlutterBindings static members
FlutterBindings::MethodCallback FlutterBindings::s_methodCallback;
bool FlutterBindings::s_initialized = false;

bool FlutterBindings::initialize() {
    if (s_initialized) {
        return true;
    }
    
    // Initialize Tienet
    if (!tienet::initialize()) {
        return false;
    }
    
    s_initialized = true;
    return true;
}

void FlutterBindings::shutdown() {
    if (s_initialized) {
        tienet::shutdown();
        s_initialized = false;
    }
}

void FlutterBindings::registerMethodChannel(const std::string& channelName) {
    // This would be implemented with actual Flutter platform channel registration
    // For now, it's a placeholder
    std::cout << "📱 Registered Flutter method channel: " << channelName << std::endl;
}

std::string FlutterBindings::handleMethodCall(const std::string& method, const std::string& args) {
    if (s_methodCallback) {
        s_methodCallback(method, args);
    }
    
    // Return a simple JSON response
    return "{\"success\": true, \"method\": \"" + method + "\"}";
}

void FlutterBindings::setMethodCallback(MethodCallback callback) {
    s_methodCallback = callback;
}

void FlutterBindings::sendEvent(const std::string& event, const std::string& data) {
    // This would send events to Flutter
    std::cout << "📤 Flutter event: " << event << " - " << data << std::endl;
}

// FlutterVRClient implementation
FlutterVRClient::FlutterVRClient() {
    m_vrClient = std::make_unique<vr::VRClient>();
}

FlutterVRClient::~FlutterVRClient() {
    FlutterBindings::shutdown();
}

bool FlutterVRClient::initialize() {
    if (!FlutterBindings::initialize()) {
        return false;
    }
    
    if (!m_vrClient->initialize()) {
        return false;
    }
    
    setupVRCallbacks();
    return true;
}

bool FlutterVRClient::connect(const std::string& serverIP, uint16_t port) {
    if (!m_vrClient) {
        return false;
    }
    
    return m_vrClient->connect(serverIP, port);
}

void FlutterVRClient::disconnect() {
    if (m_vrClient) {
        m_vrClient->disconnect();
    }
}

void FlutterVRClient::update() {
    if (m_vrClient) {
        m_vrClient->update();
    }
}

void FlutterVRClient::setBitrate(uint32_t bps) {
    if (!m_vrClient) return;
    m_vrClient->setProtocolBitrate(bps);
}

void FlutterVRClient::enablePacing(bool enabled) {
    if (!m_vrClient) return;
    m_vrClient->enableProtocolPacing(enabled);
}

void FlutterVRClient::enableFEC(bool enabled, uint32_t groupSize) {
    if (!m_vrClient) return;
    m_vrClient->enableProtocolFEC(enabled, groupSize);
}

void FlutterVRClient::setFECParity(uint32_t parityCount) {
    if (!m_vrClient) return;
    m_vrClient->setProtocolFECParity(parityCount);
}

void FlutterVRClient::setFECInterleave(uint32_t stride) {
    if (!m_vrClient) return;
    m_vrClient->setProtocolFECInterleave(stride);
}

void FlutterVRClient::setJitterBuffer(uint32_t ms) {
    if (!m_vrClient) return;
    m_vrClient->setJitterBufferMs(ms);
}

void FlutterVRClient::setMTU(uint32_t mtuBytes) {
    if (!m_vrClient) return;
    m_vrClient->setProtocolMTU(mtuBytes);
}

bool FlutterVRClient::sendGyroscopeData(float x, float y, float z) {
    if (!m_vrClient) {
        return false;
    }
    
    vr::GyroscopeData data;
    data.x = x;
    data.y = y;
    data.z = z;
    data.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    return m_vrClient->sendGyroscopeData(data);
}

bool FlutterVRClient::sendTouchData(float x, float y, uint32_t touchId, bool isPressed) {
    if (!m_vrClient) {
        return false;
    }
    
    vr::TouchData data;
    data.x = x;
    data.y = y;
    data.touchId = touchId;
    data.isPressed = isPressed;
    data.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    return m_vrClient->sendTouchData(data);
}

void FlutterVRClient::setEventCallback(EventCallback callback) {
    m_eventCallback = callback;
}

bool FlutterVRClient::isConnected() const {
    return m_vrClient ? m_vrClient->isConnected() : false;
}

std::string FlutterVRClient::getServerAddress() const {
    if (m_vrClient) {
        auto addr = m_vrClient->getServerAddress();
        return addr.host + ":" + std::to_string(addr.port);
    }
    return "";
}

void FlutterVRClient::setupVRCallbacks() {
    if (!m_vrClient) {
        return;
    }
    
    // Set VR frame callback
    m_vrClient->setFrameCallback([this](const vr::VRFrame& frame) {
        handleVRFrame(frame);
    });
    
    // Set compressed frame callback
    m_vrClient->setCompressedFrameCallback([this](const vr::CompressedVRFrame& frame) {
        handleCompressedVRFrame(frame);
    });
    
    // Set connection callbacks
    m_vrClient->setEventCallback([this](client::ClientEvent event, const core::Packet& packet) {
        switch (event) {
            case client::ClientEvent::Connected:
                handleConnection(true);
                break;
            case client::ClientEvent::Disconnected:
                handleDisconnection();
                break;
            case client::ClientEvent::Error:
                handleError("VR Client error");
                break;
            default:
                break;
        }
    });
}

void FlutterVRClient::handleVRFrame(const vr::VRFrame& frame) {
    // Convert VR frame to JSON for Flutter
    std::ostringstream json;
    json << "{"
         << "\"type\": \"vr_frame\","
         << "\"width\": " << frame.width << ","
         << "\"height\": " << frame.height << ","
         << "\"format\": " << frame.format << ","
         << "\"timestamp\": " << frame.timestamp << ","
         << "\"dataSize\": " << frame.data.size()
         << "}";
    
    if (m_eventCallback) {
        m_eventCallback("vr_frame", json.str());
    }
}

void FlutterVRClient::handleCompressedVRFrame(const vr::CompressedVRFrame& frame) {
    // Convert compressed VR frame to JSON for Flutter
    std::ostringstream json;
    json << "{"
         << "\"type\": \"compressed_vr_frame\","
         << "\"originalWidth\": " << frame.originalWidth << ","
         << "\"originalHeight\": " << frame.originalHeight << ","
         << "\"format\": " << frame.format << ","
         << "\"timestamp\": " << frame.timestamp << ","
         << "\"compressedSize\": " << frame.compressedSize
         << "}";
    
    if (m_eventCallback) {
        m_eventCallback("compressed_vr_frame", json.str());
    }
}

void FlutterVRClient::handleConnection(bool connected) {
    if (m_eventCallback) {
        std::string event = connected ? "connected" : "disconnected";
        m_eventCallback(event, "{}");
    }
}

void FlutterVRClient::handleDisconnection() {
    if (m_eventCallback) {
        m_eventCallback("disconnected", "{}");
    }
}

void FlutterVRClient::handleError(const std::string& error) {
    if (m_eventCallback) {
        std::ostringstream json;
        json << "{\"error\": \"" << error << "\"}";
        m_eventCallback("error", json.str());
    }
}

// FlutterMethodHandler implementation
FlutterMethodHandler::FlutterMethodHandler() {
}

FlutterMethodHandler::~FlutterMethodHandler() {
}

std::string FlutterMethodHandler::handleMethod(const std::string& method, const std::string& args) {
    if (method == "connect") {
        return handleConnect(args);
    } else if (method == "disconnect") {
        return handleDisconnect(args);
    } else if (method == "setBitrate") {
        if (!m_vrClient) return createErrorResponse("VR client not initialized");
        // parse args: {"bps": 4000000} (using default here)
        m_vrClient->setBitrate(4000000);
        return createSuccessResponse("{}");
    } else if (method == "enablePacing") {
        if (!m_vrClient) return createErrorResponse("VR client not initialized");
        m_vrClient->enablePacing(true);
        return createSuccessResponse("{}");
    } else if (method == "enableFEC") {
        if (!m_vrClient) return createErrorResponse("VR client not initialized");
        m_vrClient->enableFEC(true, 8);
        return createSuccessResponse("{}");
    } else if (method == "setFECParity") {
        if (!m_vrClient) return createErrorResponse("VR client not initialized");
        m_vrClient->setFECParity(1);
        return createSuccessResponse("{}");
    } else if (method == "setFECInterleave") {
        if (!m_vrClient) return createErrorResponse("VR client not initialized");
        m_vrClient->setFECInterleave(1);
        return createSuccessResponse("{}");
    } else if (method == "setJitterBuffer") {
        if (!m_vrClient) return createErrorResponse("VR client not initialized");
        m_vrClient->setJitterBuffer(30);
        return createSuccessResponse("{}");
    } else if (method == "setMTU") {
        if (!m_vrClient) return createErrorResponse("VR client not initialized");
        m_vrClient->setMTU(1400);
        return createSuccessResponse("{}");
    } else if (method == "sendGyroscopeData") {
        return handleSendGyroscopeData(args);
    } else if (method == "sendTouchData") {
        return handleSendTouchData(args);
    } else if (method == "getStatus") {
        return handleGetStatus(args);
    } else if (method == "getServerAddress") {
        return handleGetServerAddress(args);
    }
    
    return createErrorResponse("Unknown method: " + method);
}

void FlutterMethodHandler::setVRClient(std::shared_ptr<FlutterVRClient> client) {
    m_vrClient = client;
}

std::string FlutterMethodHandler::handleConnect(const std::string& args) {
    if (!m_vrClient) {
        return createErrorResponse("VR client not initialized");
    }
    
    // Parse args (JSON: {"serverIP": "127.0.0.1", "port": 27016})
    // For now, use default values
    std::string serverIP = "127.0.0.1";
    uint16_t port = 27016;
    
    if (m_vrClient->connect(serverIP, port)) {
        return createSuccessResponse("{\"connected\": true}");
    } else {
        return createErrorResponse("Failed to connect to server");
    }
}

std::string FlutterMethodHandler::handleDisconnect(const std::string& args) {
    if (!m_vrClient) {
        return createErrorResponse("VR client not initialized");
    }
    
    m_vrClient->disconnect();
    return createSuccessResponse("{\"disconnected\": true}");
}

std::string FlutterMethodHandler::handleSendGyroscopeData(const std::string& args) {
    if (!m_vrClient) {
        return createErrorResponse("VR client not initialized");
    }
    
    // Parse args (JSON: {"x": 0.0, "y": 0.0, "z": 0.0})
    // For now, use default values
    float x = 0.0f, y = 0.0f, z = 0.0f;
    
    if (m_vrClient->sendGyroscopeData(x, y, z)) {
        return createSuccessResponse("{\"sent\": true}");
    } else {
        return createErrorResponse("Failed to send gyroscope data");
    }
}

std::string FlutterMethodHandler::handleSendTouchData(const std::string& args) {
    if (!m_vrClient) {
        return createErrorResponse("VR client not initialized");
    }
    
    // Parse args (JSON: {"x": 0.0, "y": 0.0, "touchId": 0, "isPressed": false})
    // For now, use default values
    float x = 0.0f, y = 0.0f;
    uint32_t touchId = 0;
    bool isPressed = false;
    
    if (m_vrClient->sendTouchData(x, y, touchId, isPressed)) {
        return createSuccessResponse("{\"sent\": true}");
    } else {
        return createErrorResponse("Failed to send touch data");
    }
}

std::string FlutterMethodHandler::handleGetStatus(const std::string& args) {
    if (!m_vrClient) {
        return createErrorResponse("VR client not initialized");
    }
    
    std::ostringstream json;
    json << "{"
         << "\"connected\": " << (m_vrClient->isConnected() ? "true" : "false") << ","
         << "\"serverAddress\": \"" << m_vrClient->getServerAddress() << "\""
         << "}";
    
    return createSuccessResponse(json.str());
}

std::string FlutterMethodHandler::handleGetServerAddress(const std::string& args) {
    if (!m_vrClient) {
        return createErrorResponse("VR client not initialized");
    }
    
    std::string address = m_vrClient->getServerAddress();
    return createSuccessResponse("{\"address\": \"" + address + "\"}");
}

std::string FlutterMethodHandler::createResponse(bool success, const std::string& message, const std::string& data) {
    std::ostringstream json;
    json << "{"
         << "\"success\": " << (success ? "true" : "false") << ","
         << "\"message\": \"" << message << "\","
         << "\"data\": " << data
         << "}";
    return json.str();
}

std::string FlutterMethodHandler::createErrorResponse(const std::string& message) {
    return createResponse(false, message, "{}");
}

std::string FlutterMethodHandler::createSuccessResponse(const std::string& data) {
    return createResponse(true, "Success", data);
}

} // namespace flutter
} // namespace tienet
