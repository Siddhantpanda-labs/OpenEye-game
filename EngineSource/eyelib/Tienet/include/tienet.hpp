/**
 * @file tienet.hpp
 * @brief Tienet - Generic Networking Library
 * @version 1.0.0
 * @author VR Streaming Team
 * 
 * Tienet is a generic, cross-platform networking library designed for
 * real-time applications with optional VR and Flutter extensions.
 */

#pragma once

// Core networking components
#include "tienet/core/socket.hpp"
#include "tienet/core/packet.hpp"
#include "tienet/core/protocol.hpp"

// Server and client
#include "tienet/server/server.hpp"
#include "tienet/client/client.hpp"

// Compression
#include "tienet/compression/compressor.hpp"

// Streaming (UDP discovery + stream port, frame payload send, gyro callback)
#include "tienet/streaming/stream_server.hpp"

// Optional extensions
#ifdef TIENET_ENABLE_VR
#include "tienet/vr/vr_extensions.hpp"
#endif

#ifdef TIENET_ENABLE_FLUTTER
#include "tienet/flutter/flutter_bindings.hpp"
#endif

/**
 * @namespace tienet
 * @brief Main namespace for the Tienet networking library
 */
namespace tienet {

/**
 * @brief Initialize the Tienet library
 * @return true if initialization successful, false otherwise
 */
bool initialize();

/**
 * @brief Shutdown the Tienet library
 */
void shutdown();

/**
 * @brief Get the library version
 * @return Version string
 */
const char* getVersion();

/**
 * @brief Get the library build info
 * @return Build information string
 */
const char* getBuildInfo();

} // namespace tienet
