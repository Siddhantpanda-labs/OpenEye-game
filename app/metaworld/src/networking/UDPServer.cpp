/*
    Professional UDP Server Implementation for VR Streaming
    Using Valve's GameNetworkingSockets for optimal real-time performance
*/
#include "UDPServer.h"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <cstring>

VULKAN_ENGINE_NAMESPACE_BEGIN

namespace App {
    namespace Metaworld {

        UDPServer::UDPServer() 
            : m_networkingSockets(nullptr), m_networkingUtils(nullptr),
              m_listenSocket(k_HSteamListenSocket_Invalid),
              m_running(false), m_initialized(false), m_port(27016), // Streaming port only
              m_frameCounter(0), m_shouldStop(false) {
            
            // Initialize statistics
            m_stats = {0, 0, 0, 0.0f, 0};
            
            std::cout << "🎮 Initializing UDP Server with GameNetworkingSockets..." << std::endl;
        }

        UDPServer::~UDPServer() {
            shutdown();
        }

        bool UDPServer::initialize(uint16_t port) {
            if (m_initialized) {
                std::cout << "⚠️ UDP Server already initialized!" << std::endl;
                return true;
            }

            m_port = port;

            // Initialize Steam Networking (standalone mode)
            SteamDatagramErrMsg errMsg;
            if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
                std::cout << "❌ Failed to initialize GameNetworkingSockets: " << errMsg << std::endl;
                return false;
            }
            
            // Get interfaces after initialization
            m_networkingSockets = SteamNetworkingSockets();
            m_networkingUtils = SteamNetworkingUtils();
            
            if (!m_networkingSockets || !m_networkingUtils) {
                std::cout << "❌ Failed to get Steam Networking interfaces!" << std::endl;
                return false;
            }

            // Configure networking options
            SteamNetworkingConfigValue_t opt;
            opt.m_eValue = k_ESteamNetworkingConfig_LogLevel_Message;
            opt.m_eDataType = k_ESteamNetworkingConfig_Int32;
            m_networkingUtils->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_LogLevel_Message, 0);

            // Create listen socket - bind to all interfaces for broadcast discovery
            SteamNetworkingIPAddr serverAddr;
            serverAddr.Clear();
            // Bind to all interfaces (0.0.0.0) to receive broadcast messages
            serverAddr.SetIPv4(0x00000000, m_port); // 0.0.0.0:port

            SteamNetworkingConfigValue_t listenOpts[1];
            listenOpts[0].m_eValue = k_ESteamNetworkingConfig_IP_AllowWithoutAuth;
            listenOpts[0].m_eDataType = k_ESteamNetworkingConfig_Int32;
            listenOpts[0].m_val.m_int32 = 1;

            m_listenSocket = m_networkingSockets->CreateListenSocketIP(serverAddr, 1, listenOpts);
            if (m_listenSocket == k_HSteamListenSocket_Invalid) {
                std::cout << "❌ Failed to create streaming listen socket on port " << m_port << std::endl;
                return false;
            }
            
            std::cout << "✅ UDP Server listening on 0.0.0.0:" << m_port << " for VR streaming" << std::endl;
            std::cout << "   📡 Discovery handled by separate DiscoveryServer.exe" << std::endl;

            m_initialized = true;
            m_running = true;
            m_shouldStop = false;

            // Start server thread
            m_serverThread = std::thread(&UDPServer::runServer, this);
            m_frameProcessingThread = std::thread(&UDPServer::processFrames, this);
            
            std::cout << "✅ UDP Server initialized on port " << m_port << std::endl;
            return true;
        }

        void UDPServer::shutdown() {
            if (!m_initialized) return;

            std::cout << "🛑 Shutting down UDP Server..." << std::endl;

            m_shouldStop = true;
            m_running = false;

            // Stop threads
            if (m_serverThread.joinable()) {
                m_serverThread.join();
            }
            if (m_frameProcessingThread.joinable()) {
                m_frameProcessingThread.join();
            }

            // Close connections
            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                for (auto& client : m_clients) {
                    if (client->connection != k_HSteamNetConnection_Invalid) {
                        m_networkingSockets->CloseConnection(client->connection, 0, "Server shutdown", true);
                    }
                }
                m_clients.clear();
            }

            // Close listen socket
            if (m_listenSocket != k_HSteamListenSocket_Invalid) {
                m_networkingSockets->CloseListenSocket(m_listenSocket);
                m_listenSocket = k_HSteamListenSocket_Invalid;
            }

            // Shutdown GameNetworkingSockets
            GameNetworkingSockets_Kill();
            
            m_initialized = false;
            std::cout << "✅ UDP Server shutdown complete!" << std::endl;
        }

        void UDPServer::runServer() {
            std::cout << "🚀 UDP Server thread started..." << std::endl;

            while (m_running && !m_shouldStop) {
                // Process GameNetworkingSockets callbacks (handles new connections)
                m_networkingSockets->RunCallbacks();
                
                // Handle messages from existing clients
                {
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    for (auto& client : m_clients) {
                        if (client->isActive) {
                            ISteamNetworkingMessage* pMsg = nullptr;
                            int numMsgs = m_networkingSockets->ReceiveMessagesOnConnection(client->connection, &pMsg, 1);
                            
                            if (numMsgs > 0 && pMsg) {
                                std::string message(static_cast<const char*>(pMsg->GetData()), pMsg->GetSize());
                                handleClientMessage(client.get(), message);
                                pMsg->Release();
                            }
                        }
                    }
                }

                // Cleanup inactive clients
                cleanupInactiveClients();
                
                // Update statistics
                updateStats();
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            std::cout << "🏁 UDP Server thread ended." << std::endl;
        }


        void UDPServer::processFrames() {
            std::cout << "🎬 Frame processing thread started..." << std::endl;

            while (!m_shouldStop) {
                std::vector<FrameData> framesToProcess;
                
                // Get frames from queue
                {
                    std::lock_guard<std::mutex> lock(m_frameQueueMutex);
                    while (!m_frameQueue.empty()) {
                        framesToProcess.push_back(m_frameQueue.front());
                        m_frameQueue.pop();
                    }
                }

                // Process frames
                for (const auto& frame : framesToProcess) {
                    if (m_frameCallback) {
                        m_frameCallback(frame);
                    }
                    
                    // Broadcast frame to all clients
                    broadcastFrame(frame);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            std::cout << "🏁 Frame processing thread ended." << std::endl;
        }

        void UDPServer::handleClientMessage(VRClient* client, const std::string& message) {
            try {
                std::cout << "📨 Received message from client: " << message << std::endl;
                
                // Handle discovery requests
                if (message == "VR_SERVER_DISCOVERY_REQUEST") {
                    handleDiscoveryRequest(client);
                    return;
                }
                
                // Handle client connection requests
                if (message == "VR_CLIENT_CONNECT") {
                    handleClientConnect(client);
                    return;
                }
                
                // Simple JSON parsing (in production, use a proper JSON library)
                if (message.find("\"type\":\"gyro\"") != std::string::npos) {
                    handleGyroscopeData(message);
                } else if (message.find("\"type\":\"touch\"") != std::string::npos) {
                    handleTouchData(message);
                } else if (message.find("\"type\":\"ping\"") != std::string::npos) {
                    handlePing(message);
                }
            } catch (const std::exception& e) {
                std::cout << "⚠️ Error handling client message: " << e.what() << std::endl;
            }
        }

        void UDPServer::handleGyroscopeData(const std::string& data) {
            // Simple parsing - in production, use proper JSON parsing
            if (m_gyroCallback) {
                // Extract x, y, z values (simplified)
                float x = 0.0f, y = 0.0f, z = 0.0f;
                // TODO: Parse JSON properly
                m_gyroCallback(x, y, z);
            }
        }

        void UDPServer::handleTouchData(const std::string& data) {
            if (m_touchCallback) {
                float x = 0.0f, y = 0.0f;
                bool pressed = false;
                // TODO: Parse JSON properly
                m_touchCallback(x, y, pressed);
            }
        }

        void UDPServer::handlePing(const std::string& data) {
            // Update client latency
            // TODO: Implement proper ping handling
        }

        void UDPServer::handleClientConnect(VRClient* client) {
            std::cout << "🔗 Client connection request received from " << client->address << ":" << client->port << std::endl;
            
            // Send connection acknowledgment
            std::string response = "VR_SERVER_CONNECTED";
            m_networkingSockets->SendMessageToConnection(
                client->connection,
                response.c_str(),
                response.length(),
                k_nSteamNetworkingSend_Reliable,
                nullptr
            );
            
            std::cout << "✅ Client connected successfully: " << client->address << ":" << client->port << std::endl;
        }

        void UDPServer::handleDiscoveryRequest(VRClient* client) {
            std::cout << "🔍 Received server discovery request from client" << std::endl;
            
            // Send discovery response with streaming port
            std::string response = "VR_SERVER_DISCOVERY_RESPONSE:" + std::to_string(m_port) + ":MetaverseGameVR";
            m_networkingSockets->SendMessageToConnection(
                client->connection,
                response.c_str(),
                response.length(),
                k_nSteamNetworkingSend_Reliable,
                nullptr
            );
            
            std::cout << "📡 Sent discovery response: " << response << " to " << client->address << std::endl;
        }

        void UDPServer::handleDiscoveryConnection(HSteamNetConnection conn) {
            std::cout << "🔍 New discovery connection established" << std::endl;
            
            // Create a temporary client for discovery
            auto discoveryClient = std::make_unique<VRClient>();
            discoveryClient->clientId = generateClientId();
            discoveryClient->connection = conn;
            discoveryClient->isActive = true;
            discoveryClient->lastPing = std::chrono::high_resolution_clock::now();
            
            // Get connection info
            SteamNetConnectionInfo_t info;
            m_networkingSockets->GetConnectionInfo(conn, &info);
            
            char addrStr[256];
            info.m_addrRemote.ToString(addrStr, sizeof(addrStr), true);
            discoveryClient->address = addrStr;
            discoveryClient->port = info.m_addrRemote.m_port;
            
            std::cout << "📡 Discovery client connected from " << discoveryClient->address << ":" << discoveryClient->port << std::endl;
            
            // Store discovery client temporarily
            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                m_clients.push_back(std::move(discoveryClient));
            }
        }

        void UDPServer::broadcastMessage(const std::string& message) {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            for (const auto& client : m_clients) {
                if (client->isActive) {
                    m_networkingSockets->SendMessageToConnection(
                        client->connection, 
                        message.c_str(), 
                        message.length(), 
                        k_nSteamNetworkingSend_Reliable, 
                        nullptr
                    );
                }
            }
        }

        void UDPServer::sendToClient(const std::string& clientId, const std::string& message) {
            VRClient* client = findClient(clientId);
            if (client && client->isActive) {
                m_networkingSockets->SendMessageToConnection(
                    client->connection, 
                    message.c_str(), 
                    message.length(), 
                    k_nSteamNetworkingSend_Reliable, 
                    nullptr
                );
            }
        }

        void UDPServer::streamFrame(const std::vector<uint8_t>& frameData, uint32_t width, uint32_t height, bool isStereo) {
            FrameData frame;
            frame.data = frameData;
            frame.width = width;
            frame.height = height;
            frame.isStereo = isStereo;
            frame.frameId = m_frameCounter++;
            frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

            {
                std::lock_guard<std::mutex> lock(m_frameQueueMutex);
                m_frameQueue.push(frame);
            }
        }

        void UDPServer::broadcastFrame(const FrameData& frame) {
            std::string frameMessage;
            createFrameMessage(frame, frameMessage);
            broadcastMessage(frameMessage);
        }

        void UDPServer::setFrameCallback(std::function<void(const FrameData&)> callback) {
            m_frameCallback = callback;
        }

        void UDPServer::setGyroscopeCallback(std::function<void(float, float, float)> callback) {
            m_gyroCallback = callback;
        }

        void UDPServer::setTouchCallback(std::function<void(float, float, bool)> callback) {
            m_touchCallback = callback;
        }

        UDPServer::ServerStats UDPServer::getStats() const {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            return m_stats;
        }

        VRClient* UDPServer::findClient(const std::string& clientId) {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            for (const auto& client : m_clients) {
                if (client->clientId == clientId) {
                    return client.get();
                }
            }
            return nullptr;
        }

        VRClient* UDPServer::findClient(HSteamNetConnection connection) {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            for (const auto& client : m_clients) {
                if (client->connection == connection) {
                    return client.get();
                }
            }
            return nullptr;
        }

        std::string UDPServer::generateClientId() {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<> dis(1000, 9999);
            return "VR_" + std::to_string(dis(gen));
        }

        void UDPServer::cleanupInactiveClients() {
            auto now = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            
            m_clients.erase(
                std::remove_if(m_clients.begin(), m_clients.end(),
                    [&](const std::unique_ptr<VRClient>& client) {
                        auto timeSinceLastPing = std::chrono::duration_cast<std::chrono::seconds>(now - client->lastPing);
                        if (timeSinceLastPing.count() > 30) { // 30 second timeout
                            if (client->connection != k_HSteamNetConnection_Invalid) {
                                m_networkingSockets->CloseConnection(client->connection, 0, "Client timeout", true);
                            }
                            return true;
                        }
                        return false;
                    }),
                m_clients.end()
            );
        }

        void UDPServer::updateClientLatency(VRClient* client) {
            // TODO: Implement proper latency measurement
            client->latency = 0.0f;
        }

        void UDPServer::compressFrame(const FrameData& frame, std::vector<uint8_t>& compressed) {
            // TODO: Implement frame compression (H.264/H.265)
            compressed = frame.data;
        }

        void UDPServer::createFrameMessage(const FrameData& frame, std::string& message) {
            // Create frame message (simplified)
            std::ostringstream oss;
            oss << "{\"type\":\"frame\",\"frameId\":" << frame.frameId 
                << ",\"width\":" << frame.width << ",\"height\":" << frame.height
                << ",\"stereo\":" << (frame.isStereo ? "true" : "false") << "}";
            message = oss.str();
        }

        void UDPServer::sendFrameToClientInternal(VRClient* client, const FrameData& frame) {
            if (client && client->isActive) {
                std::string frameMessage;
                createFrameMessage(frame, frameMessage);
                sendToClient(client->clientId, frameMessage);
            }
        }

        void UDPServer::updateStats() {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.connectedClients = static_cast<uint32_t>(m_clients.size());
            // TODO: Update other statistics
        }

        std::vector<VRClient*> UDPServer::getConnectedClients() const {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_clientsMutex));
            std::vector<VRClient*> clients;
            for (const auto& client : m_clients) {
                clients.push_back(client.get());
            }
            return clients;
        }

        // Global callback for connection status changes
        void SteamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* pInfo) {
            switch (pInfo->m_info.m_eState) {
                case k_ESteamNetworkingConnectionState_Connecting:
                    {
                        char addrStr[256];
                        pInfo->m_info.m_addrRemote.ToString(addrStr, sizeof(addrStr), true);
                        std::cout << "🔗 Client connecting from " << addrStr << std::endl;
                    }
                    break;
                    
                case k_ESteamNetworkingConnectionState_Connected:
                    {
                        char addrStr[256];
                        pInfo->m_info.m_addrRemote.ToString(addrStr, sizeof(addrStr), true);
                        std::cout << "✅ Client connected: " << addrStr << std::endl;
                        
                        // Determine if this is discovery or streaming connection based on listen socket
                        // We'll handle this in the main server loop by checking which socket the connection came from
                    }
                    break;
                    
                case k_ESteamNetworkingConnectionState_ClosedByPeer:
                case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
                    {
                        char addrStr[256];
                        pInfo->m_info.m_addrRemote.ToString(addrStr, sizeof(addrStr), true);
                        std::cout << "❌ Client disconnected: " << addrStr << std::endl;
                    }
                    break;
            }
        }

    } // namespace Metaworld
} // namespace App

VULKAN_ENGINE_NAMESPACE_END
