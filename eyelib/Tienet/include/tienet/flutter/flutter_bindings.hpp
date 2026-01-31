/**
 * @file flutter_bindings.hpp
 * @brief Flutter platform channel bindings for Tienet
 */

#pragma once

#include "tienet/client/client.hpp"
#include "tienet/vr/vr_extensions.hpp"
#include <string>
#include <functional>
#include <memory>

namespace tienet {
namespace flutter {

/**
 * @brief Flutter method channel handler
 */
class FlutterBindings {
public:
    using MethodCallback = std::function<void(const std::string& method, const std::string& args)>;
    
    /**
     * @brief Initialize Flutter bindings
     * @return true if successful, false otherwise
     */
    static bool initialize();
    
    /**
     * @brief Shutdown Flutter bindings
     */
    static void shutdown();
    
    /**
     * @brief Register method channel
     * @param channelName Channel name
     */
    static void registerMethodChannel(const std::string& channelName = "tienet");
    
    /**
     * @brief Handle method call from Flutter
     * @param method Method name
     * @param args Method arguments (JSON string)
     * @return Response (JSON string)
     */
    static std::string handleMethodCall(const std::string& method, const std::string& args);
    
    /**
     * @brief Set method callback
     * @param callback Method callback function
     */
    static void setMethodCallback(MethodCallback callback);
    
    /**
     * @brief Send event to Flutter
     * @param event Event name
     * @param data Event data (JSON string)
     */
    static void sendEvent(const std::string& event, const std::string& data);

private:
    static MethodCallback s_methodCallback;
    static bool s_initialized;
};

/**
 * @brief Flutter VR client wrapper
 */
class FlutterVRClient {
public:
    using EventCallback = std::function<void(const std::string& event, const std::string& data)>;
    
    FlutterVRClient();
    virtual ~FlutterVRClient();
    
    /**
     * @brief Initialize Flutter VR client
     * @return true if successful, false otherwise
     */
    virtual bool initialize();
    
    /**
     * @brief Connect to VR server
     * @param serverIP Server IP address
     * @param port Server port
     * @return true if successful, false otherwise
     */
    virtual bool connect(const std::string& serverIP, uint16_t port);
    
    /**
     * @brief Disconnect from server
     */
    virtual void disconnect();
    
    /**
     * @brief Update client
     */
    virtual void update();
    // Runtime knobs
    virtual void setBitrate(uint32_t bps);
    virtual void enablePacing(bool enabled);
    virtual void enableFEC(bool enabled, uint32_t groupSize = 8);
    // New knobs
    virtual void setFECParity(uint32_t parityCount);
    virtual void setFECInterleave(uint32_t stride);
    virtual void setJitterBuffer(uint32_t ms);
    virtual void setMTU(uint32_t mtuBytes);
    
    /**
     * @brief Send gyroscope data
     * @param x X axis value
     * @param y Y axis value
     * @param z Z axis value
     * @return true if successful, false otherwise
     */
    virtual bool sendGyroscopeData(float x, float y, float z);
    
    /**
     * @brief Send touch data
     * @param x Touch X coordinate
     * @param y Touch Y coordinate
     * @param touchId Touch identifier
     * @param isPressed Touch state
     * @return true if successful, false otherwise
     */
    virtual bool sendTouchData(float x, float y, uint32_t touchId, bool isPressed);
    
    /**
     * @brief Set event callback
     * @param callback Event callback function
     */
    virtual void setEventCallback(EventCallback callback);
    
    /**
     * @brief Check if connected
     * @return true if connected
     */
    virtual bool isConnected() const;
    
    /**
     * @brief Get server address
     * @return Server address
     */
    virtual std::string getServerAddress() const;

private:
    std::unique_ptr<vr::VRClient> m_vrClient;
    EventCallback m_eventCallback;
    
    void setupVRCallbacks();
    void handleVRFrame(const vr::VRFrame& frame);
    void handleCompressedVRFrame(const vr::CompressedVRFrame& frame);
    void handleConnection(bool connected);
    void handleDisconnection();
    void handleError(const std::string& error);
};

/**
 * @brief Flutter method call handler
 */
class FlutterMethodHandler {
public:
    FlutterMethodHandler();
    virtual ~FlutterMethodHandler();
    
    /**
     * @brief Handle method call
     * @param method Method name
     * @param args Method arguments
     * @return Response
     */
    virtual std::string handleMethod(const std::string& method, const std::string& args);
    
    /**
     * @brief Set VR client
     * @param client VR client instance
     */
    virtual void setVRClient(std::shared_ptr<FlutterVRClient> client);

private:
    std::shared_ptr<FlutterVRClient> m_vrClient;
    
    std::string handleConnect(const std::string& args);
    std::string handleDisconnect(const std::string& args);
    std::string handleSendGyroscopeData(const std::string& args);
    std::string handleSendTouchData(const std::string& args);
    std::string handleGetStatus(const std::string& args);
    std::string handleGetServerAddress(const std::string& args);
    
    std::string createResponse(bool success, const std::string& message = "", const std::string& data = "");
    std::string createErrorResponse(const std::string& message);
    std::string createSuccessResponse(const std::string& data = "");
};

} // namespace flutter
} // namespace tienet
