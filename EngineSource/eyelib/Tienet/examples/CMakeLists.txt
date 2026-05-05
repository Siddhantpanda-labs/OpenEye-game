# Tienet Examples
cmake_minimum_required(VERSION 3.16)

# Basic server example
add_executable(basic_server basic_server.cpp)
target_link_libraries(basic_server PRIVATE Tienet)

# Basic client example
add_executable(basic_client basic_client.cpp)
target_link_libraries(basic_client PRIVATE Tienet)

# VR server example
add_executable(vr_server vr_server.cpp)
target_link_libraries(vr_server PRIVATE Tienet)

# Discovery example
add_executable(discovery_example discovery_example.cpp)
target_link_libraries(discovery_example PRIVATE Tienet)

# Media transport example
add_executable(media_transport_example media_transport_example.cpp)
target_link_libraries(media_transport_example PRIVATE Tienet)

# Set output directory
set_target_properties(basic_server basic_client vr_server discovery_example media_transport_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
