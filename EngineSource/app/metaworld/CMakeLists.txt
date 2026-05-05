# Metaverse Game CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(MetaverseGame)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${OPENEYE_ENGINE_SOURCE}/include)
include_directories(${OPENEYE_ENGINE_SOURCE}/thirdparty/gamenetworkingsockets/include)

# Source files - Main VR streaming app with GameNetworkingSockets
set(METAVERSE_SOURCES
    src/main.cpp
    src/networking/UDPServer.cpp
    src/networking/VRStreamingManager.cpp
    src/networking/FrameCapture.cpp
)

# Source files - Discovery server with raw UDP
set(DISCOVERY_SOURCES
    src/discovery/main.cpp
    src/discovery/DiscoveryServer.cpp
)

# Create executables
add_executable(MetaverseGame ${METAVERSE_SOURCES})
add_executable(DiscoveryServer ${DISCOVERY_SOURCES})

# Link against the Vulkan Engine library and GameNetworkingSockets
target_link_libraries(MetaverseGame PRIVATE VulkanEngine GameNetworkingSockets)

# Link discovery server with Windows sockets
target_link_libraries(DiscoveryServer PRIVATE ws2_32)

# Set output directory
set_target_properties(MetaverseGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(DiscoveryServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Copy assets to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR}/bin)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/scenes DESTINATION ${CMAKE_BINARY_DIR}/bin)

# Set working directory for debugging
set_target_properties(MetaverseGame PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
