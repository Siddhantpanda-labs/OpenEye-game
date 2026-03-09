#include "DiscoveryServer.h"
#include <iostream>
#include <chrono>

namespace App {
    namespace Metaworld {

        DiscoveryServer::DiscoveryServer() 
            : m_discoverySocket(INVALID_SOCKET), m_running(false), m_initialized(false),
              m_namedPipe(INVALID_HANDLE_VALUE), m_pipeName("\\\\.\\pipe\\MetaverseDiscovery") {
            
            std::cout << "🔍 Metaverse Discovery Server Starting..." << std::endl;
            std::cout << "🔍 Initializing Discovery Server..." << std::endl;
            
            // Get server name from user
            std::cout << "\n📝 Enter server name: ";
            std::getline(std::cin, m_serverName);
            
            // If empty, use default name
            if (m_serverName.empty()) {
                m_serverName = "MetaverseGameVR";
            }
            
            std::cout << "✅ Server name set to: \"" << m_serverName << "\"" << std::endl;
        }

        DiscoveryServer::~DiscoveryServer() {
            shutdown();
        }

        bool DiscoveryServer::initialize() {
            // Initialize Winsock
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                std::cout << "❌ WSAStartup failed: " << result << std::endl;
                return false;
            }

            // Create UDP socket
            m_discoverySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (m_discoverySocket == INVALID_SOCKET) {
                std::cout << "❌ Failed to create discovery socket: " << WSAGetLastError() << std::endl;
                WSACleanup();
                return false;
            }

            // Enable broadcast
            int broadcast = 1;
            if (setsockopt(m_discoverySocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast, sizeof(broadcast)) == SOCKET_ERROR) {
                std::cout << "❌ Failed to enable broadcast: " << WSAGetLastError() << std::endl;
                closesocket(m_discoverySocket);
                WSACleanup();
                return false;
            }

            // Auto-detect best network interface and bind
            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(27015);
            
            // Try to bind to all interfaces first (most compatible)
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            if (bind(m_discoverySocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                std::cout << "❌ Failed to bind discovery socket: " << WSAGetLastError() << std::endl;
                closesocket(m_discoverySocket);
                WSACleanup();
                return false;
            }
            std::cout << "✅ Bound to all interfaces (auto-detected)" << std::endl;

            // Get local IP address for debugging
            char hostname[256];
            if (gethostname(hostname, sizeof(hostname)) == 0) {
                struct addrinfo hints, *info;
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                
                if (getaddrinfo(hostname, NULL, &hints, &info) == 0) {
                    struct sockaddr_in* addr = (struct sockaddr_in*)info->ai_addr;
                    std::cout << "   🌐 Local IP: " << inet_ntoa(addr->sin_addr) << std::endl;
                    freeaddrinfo(info);
                }
            }

            // Create named pipe for communication with main process
            if (!createNamedPipe()) {
                std::cout << "❌ Failed to create named pipe" << std::endl;
                closesocket(m_discoverySocket);
                WSACleanup();
                return false;
            }

            m_initialized = true;
            m_running = true;

            std::cout << "✅ Discovery Server initialized successfully!" << std::endl;
            std::cout << "   📡 Listening on 0.0.0.0:27015 for discovery requests" << std::endl;
            std::cout << "   🔗 Named pipe: " << m_pipeName << std::endl;

            return true;
        }

        void DiscoveryServer::shutdown() {
            if (m_running) {
                m_running = false;
                
                if (m_discoverySocket != INVALID_SOCKET) {
                    closesocket(m_discoverySocket);
                    m_discoverySocket = INVALID_SOCKET;
                }
                
                closeNamedPipe();
                WSACleanup();
                
                std::cout << "🛑 Discovery Server shutdown complete!" << std::endl;
            }
        }

        void DiscoveryServer::run() {
            std::cout << "🚀 Discovery Server started..." << std::endl;

            char buffer[1024];
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            auto lastStatusTime = std::chrono::high_resolution_clock::now();

            while (m_running) {
                auto currentTime = std::chrono::high_resolution_clock::now();
                
                // Print status every 5 seconds
                if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastStatusTime).count() >= 5) {
                    std::cout << "💓 Discovery Server alive - waiting for broadcasts..." << std::endl;
                    lastStatusTime = currentTime;
                }
                
                // Set socket to non-blocking for better responsiveness
                u_long mode = 1;
                ioctlsocket(m_discoverySocket, FIONBIO, &mode);
                
                // Receive discovery request
                int bytesReceived = recvfrom(m_discoverySocket, buffer, sizeof(buffer) - 1, 0, 
                                           (sockaddr*)&clientAddr, &clientAddrLen);
                
                if (bytesReceived > 0) {
                    buffer[bytesReceived] = '\0';
                    std::string message(buffer);
                    
                    std::cout << "📨 Discovery request from " << inet_ntoa(clientAddr.sin_addr) 
                              << ":" << ntohs(clientAddr.sin_port) << " - " << message << std::endl;
                    
                    if (message == "VR_SERVER_DISCOVERY_REQUEST") {
                        handleDiscoveryRequest(clientAddr);
                    }
                } else if (bytesReceived == SOCKET_ERROR) {
                    int error = WSAGetLastError();
                    if (error != WSAEWOULDBLOCK) {
                        std::cout << "❌ Socket error: " << error << std::endl;
                    }
                }
                
                // Reset to blocking mode for recvfrom
                mode = 0;
                ioctlsocket(m_discoverySocket, FIONBIO, &mode);
                
                // Small delay to prevent busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            std::cout << "🏁 Discovery Server stopped." << std::endl;
        }

        void DiscoveryServer::handleDiscoveryRequest(const sockaddr_in& clientAddr) {
            // Send discovery response with custom server name
            std::string response = "VR_SERVER_DISCOVERY_RESPONSE:27016:" + m_serverName;
            
            int bytesSent = sendto(m_discoverySocket, response.c_str(), response.length(), 0,
                                 (sockaddr*)&clientAddr, sizeof(clientAddr));
            
            if (bytesSent > 0) {
                std::cout << "📡 Discovery response sent: " << response << std::endl;
                
                // Notify main process about new client
                std::string clientInfo = inet_ntoa(clientAddr.sin_addr);
                clientInfo += ":";
                clientInfo += std::to_string(ntohs(clientAddr.sin_port));
                
                notifyMainProcess(clientInfo);
            } else {
                std::cout << "❌ Failed to send discovery response: " << WSAGetLastError() << std::endl;
            }
        }

        void DiscoveryServer::notifyMainProcess(const std::string& clientInfo) {
            if (m_namedPipe != INVALID_HANDLE_VALUE) {
                DWORD bytesWritten;
                std::string message = "CLIENT_DISCOVERED:" + clientInfo + "\n";
                
                if (WriteFile(m_namedPipe, message.c_str(), message.length(), &bytesWritten, NULL)) {
                    std::cout << "📤 Notified main process: " << clientInfo << std::endl;
                } else {
                    std::cout << "❌ Failed to notify main process: " << GetLastError() << std::endl;
                }
            }
        }

        bool DiscoveryServer::createNamedPipe() {
            m_namedPipe = CreateNamedPipeA(
                m_pipeName.c_str(),
                PIPE_ACCESS_OUTBOUND,
                PIPE_TYPE_MESSAGE | PIPE_WAIT,
                1,
                1024,
                1024,
                0,
                NULL
            );
            
            return m_namedPipe != INVALID_HANDLE_VALUE;
        }

        void DiscoveryServer::closeNamedPipe() {
            if (m_namedPipe != INVALID_HANDLE_VALUE) {
                CloseHandle(m_namedPipe);
                m_namedPipe = INVALID_HANDLE_VALUE;
            }
        }

    } // namespace Metaworld
} // namespace App
