#include <tienet.hpp>
#include <tienet/discovery/discovery.hpp>
#include <iostream>
#include <thread>

using namespace tienet;

int main() {
	if (!tienet::initialize()) {
		std::cerr << "Failed to initialize Tienet" << std::endl;
		return -1;
	}

    discovery::DiscoveryServer server;
    discovery::DiscoveryServerConfig scfg;
    scfg.discoveryPort = 27015;
    scfg.servicePort = 27016;
    scfg.serviceName = "TienetVRServer";
    server.initialize(scfg);
    server.start();

    discovery::DiscoveryClient client;
    client.initialize(27015);
    auto results = client.discover(1000);
    for (const auto& info : results) {
        std::cout << "Found server: " << info.serviceName << " at "
                  << info.fromAddress.host << ":" << info.servicePort << std::endl;
    }

	server.stop();
	tienet::shutdown();
	return 0;
}
