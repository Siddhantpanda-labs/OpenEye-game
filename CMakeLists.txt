cmake_minimum_required(VERSION 3.20)
project(OpenEye VERSION 0.1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(MSVC)
    add_compile_options(/W4 /WX /permissive- /Zc:preprocessor)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # Enable parallel compilation
    add_compile_options(/MP)
    # Disable warnings for third-party code
    add_compile_options(/wd4245 /wd4100 /wd4189 /wd4244 /wd4034 /wd4127)
else()
    add_compile_options(-Wall -Wextra -Werror -pedantic)
endif()

# Build configuration
set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Options
option(OPENEYE_BUILD_EDITOR "Build development editor" ON)
option(OPENEYE_BUILD_GAME "Build final game" ON)

# Global include directories (components will add their own)
# These are for legacy compatibility and third-party includes

# --------------------------------------
# GLAD (match GameEngine approach - simple static library)
# --------------------------------------
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/thirdparty/glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/glad/include)

# --------------------------------------
# GLFW (use GLFW's own CMake build system - proper way)
# --------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/glfw)

# --------------------------------------
# Bullet3 Physics Library
# --------------------------------------
# Configure Bullet3 build options
set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(USE_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(BULLET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BULLET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
# Disable warnings for Bullet3 (third-party code)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/bullet3)
# Suppress warnings for Bullet3 targets (must be after add_subdirectory)
if(MSVC)
    if(TARGET BulletDynamics)
        target_compile_options(BulletDynamics PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletDynamics PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET BulletCollision)
        target_compile_options(BulletCollision PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletCollision PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET LinearMath)
        target_compile_options(LinearMath PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(LinearMath PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    # Suppress warnings for additional Bullet3 libraries
    if(TARGET BulletSoftBody)
        target_compile_options(BulletSoftBody PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456 /wd4458 /wd4459 /wd4463 /wd4756 /wd4018 /wd4389)
        set_target_properties(BulletSoftBody PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET BulletInverseDynamics)
        target_compile_options(BulletInverseDynamics PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletInverseDynamics PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET Bullet3Common)
        target_compile_options(Bullet3Common PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456 /wd4458)
        set_target_properties(Bullet3Common PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    # Suppress warnings for BulletRobotics (extras library) if it exists
    if(TARGET BulletRobotics)
        target_compile_options(BulletRobotics PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456 /wd5054)
        set_target_properties(BulletRobotics PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
endif()

# ImGui library (shared across editor and game)
set(IMGUI_SOURCES
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)

set(IMGUIZMO_SOURCES
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImGuizmo.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImCurveEdit.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImGradient.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/ImSequencer.cpp
    ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo/GraphEditor.cpp
)

add_library(OpenEyeImGui STATIC
    ${IMGUI_SOURCES}
    ${IMGUIZMO_SOURCES}
)

target_include_directories(OpenEyeImGui
    PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/imgui
        ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends
        ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo
)

target_link_libraries(OpenEyeImGui
    PUBLIC
        glad
        glfw
)

# --------------------------------------
# Component Libraries (Modular Structure)
# --------------------------------------
# Add component subdirectories in dependency order
add_subdirectory(src/core)
add_subdirectory(src/assets)
add_subdirectory(src/rendering)
add_subdirectory(src/animation)
add_subdirectory(src/particles)
add_subdirectory(src/physics)
add_subdirectory(src/fluids)
add_subdirectory(src/ecs)

# --------------------------------------
# Editor Application (Development Environment)
# --------------------------------------
# Editor only builds in Debug configuration
if(OPENEYE_BUILD_EDITOR)
    # Legacy editor (src/editor/main.cpp) - for backward compatibility
    add_executable(Editor
        src/editor/main.cpp
    )
    
    target_include_directories(Editor
        PRIVATE
            ${CMAKE_SOURCE_DIR}/thirdparty/imgui
            ${CMAKE_SOURCE_DIR}/thirdparty/imgui/backends
            ${CMAKE_SOURCE_DIR}/thirdparty/ImGuizmo
            ${CMAKE_SOURCE_DIR}/thirdparty/glm
            ${CMAKE_SOURCE_DIR}/thirdparty/assimp/include
            ${CMAKE_SOURCE_DIR}/thirdparty/bullet3/src
    )
    
    target_link_libraries(Editor
        PRIVATE
            OpenEyeECS
            OpenEyeImGui
    )
    
    # Debug configuration: enable editor features
    target_compile_definitions(Editor PRIVATE OPENEYE_EDITOR=1)
    
    # Editor panels library (used by OpenMeta in Debug builds)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
        add_subdirectory(app/editor)
    endif()
endif()

# --------------------------------------
# Game Application (Final Runtime)
# --------------------------------------
# Game application in app/openmeta/
# Game never depends on editor - clean separation
if(OPENEYE_BUILD_GAME)
    add_subdirectory(app/openmeta)
endif()


