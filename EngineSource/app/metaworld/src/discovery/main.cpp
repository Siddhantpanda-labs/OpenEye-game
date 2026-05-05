#include "DiscoveryServer.h"
#include <iostream>

int main() {
    std::cout << "🔍 Metaverse Discovery Server Starting..." << std::endl;
    
    App::Metaworld::DiscoveryServer discoveryServer;
    
    if (!discoveryServer.initialize()) {
        std::cout << "❌ Failed to initialize discovery server!" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Discovery Server ready! Press Ctrl+C to stop." << std::endl;
    
    // Run the discovery server
    discoveryServer.run();
    
    return 0;
}
