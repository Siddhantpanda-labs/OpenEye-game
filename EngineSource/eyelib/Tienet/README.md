# Tienet - Generic Networking Library

Tienet is a modern, cross-platform networking library designed for real-time applications with optional VR and Flutter extensions.

## Features

- **Generic Core**: Cross-platform socket abstraction, packet system, and protocol handling
- **VR Extensions**: Built-in support for VR frame streaming, gyroscope data, and touch input
- **Flutter Integration**: Platform channel bindings for Flutter applications
- **Compression**: Multiple compression algorithms (LZ4, Zstandard, None)
- **Reliability**: Packet acknowledgment, fragmentation, and error handling
- **Performance**: Optimized for low-latency, high-throughput applications

## Quick Start

### Basic Server

```cpp
#include <tienet.hpp>

int main() {
    tienet::initialize();
    
    tienet::server::UDPServer server;
    server.initialize({.port = 8080});
    server.start();
    
    // Main loop
    while (running) {
        server.update();
    }
    
    server.stop();
    tienet::shutdown();
    return 0;
}
```

### Basic Client

```cpp
#include <tienet.hpp>

int main() {
    tienet::initialize();
    
    tienet::client::UDPClient client;
    client.initialize({.serverAddress = "127.0.0.1", .serverPort = 8080});
    client.connect("127.0.0.1", 8080);
    
    // Main loop
    while (running) {
        client.update();
    }
    
    client.disconnect();
    tienet::shutdown();
    return 0;
}
```

### VR Server

```cpp
#include <tienet.hpp>

int main() {
    tienet::initialize();
    
    tienet::vr::VRServer server;
    server.initialize({.port = 27016, .enableCompression = true});
    server.start();
    
    // Send VR frame
    tienet::vr::VRFrame frame(1280, 720, 0);
    // ... populate frame data ...
    server.sendFrame(frame);
    
    server.stop();
    tienet::shutdown();
    return 0;
}
```

## Building

### CMake Integration

```cmake
# Add Tienet to your project
add_subdirectory(path/to/Tienet)
target_link_libraries(your_target PRIVATE Tienet)
```

### Standalone Build

```bash
mkdir build
cd build
cmake ..
make
```

## Examples

- `basic_server.cpp` - Simple UDP server
- `basic_client.cpp` - Simple UDP client
- `vr_server.cpp` - VR frame streaming server

## Architecture

```
Tienet/
├── Core/           # Generic networking components
├── Server/         # Server implementations
├── Client/         # Client implementations
├── Compression/    # Data compression
├── VR/            # VR-specific extensions
└── Flutter/       # Flutter platform bindings
```

## Requirements

- C++17 or later
- CMake 3.16 or later
- Thread support

### Optional Dependencies

- LZ4 (for compression)
- Zstandard (for compression)

## License

MIT License - see LICENSE file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests
5. Submit a pull request

## Support

For questions and support, please open an issue on GitHub.
