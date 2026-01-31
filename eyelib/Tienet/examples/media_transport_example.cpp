#include <tienet.hpp>
#include <tienet/vr/vr_extensions.hpp>
#include <iostream>
#include <thread>

using namespace tienet;

int main() {
	if (!tienet::initialize()) {
		std::cerr << "Failed to initialize Tienet" << std::endl;
		return -1;
	}

	vr::VRServer vrServer;
	vr::VRServerConfig sc;
	sc.port = 27016;
	vrServer.initialize(sc);
	vrServer.setFrameCallback([](const vr::VRFrame& f){
		std::cout << "Server received frame ts=" << f.timestamp << " size=" << f.data.size() << std::endl;
	});
	vrServer.start();

	vr::VRClient vrClient;
	vr::VRClientConfig cc;
	cc.serverAddress = "127.0.0.1";
	cc.serverPort = 27016;
	vrClient.initialize(cc);
	vrClient.setFrameCallback([](const vr::VRFrame& f){
		std::cout << "Client played frame ts=" << f.timestamp << " size=" << f.data.size() << std::endl;
	});
	vrClient.connect(cc.serverAddress, cc.serverPort);

	// Build a dummy frame
	vr::VRFrame frame(320, 180, 4);
	frame.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
	frame.data.resize(frame.width * frame.height * 4, 123);

	// Let connections settle
	for (int i = 0; i < 10; ++i) { vrServer.update(); vrClient.update(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }

	vrServer.sendFrame(frame);

	// Pump updates for a short while to process send/receive
	for (int i = 0; i < 50; ++i) { vrServer.update(); vrClient.update(); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }

	vrClient.disconnect();
	vrServer.stop();
	tienet::shutdown();
	return 0;
}
