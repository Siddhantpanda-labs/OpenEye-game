#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

namespace App {
    namespace Metaworld {

        class DiscoveryServer {
        public:
            DiscoveryServer();
            ~DiscoveryServer();

            bool initialize();
            void shutdown();
            void run();

        private:
            SOCKET m_discoverySocket;
            std::string m_serverName;
            std::atomic<bool> m_running;
            std::atomic<bool> m_initialized;
            
            // Process communication
            HANDLE m_namedPipe;
            std::string m_pipeName;
            
            void handleDiscoveryRequest(const sockaddr_in& clientAddr);
            void notifyMainProcess(const std::string& clientInfo);
            bool createNamedPipe();
            void closeNamedPipe();
        };

    } // namespace Metaworld
} // namespace App
