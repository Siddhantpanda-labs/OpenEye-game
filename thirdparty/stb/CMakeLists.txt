# thirdparty/stb/CMakeLists.txt

# Define stb as a header-only library
add_library(stb INTERFACE)

# Set the include path so anyone linking to stb can find headers
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Optional: set namespace explicitly for clarity in some setups (not always needed)
# This allows usage as stb::stb
add_library(stb::stb ALIAS stb)