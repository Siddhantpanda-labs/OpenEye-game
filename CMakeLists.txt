cmake_minimum_required(VERSION 3.20)
project(OpenEye VERSION 0.1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor)
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
option(OPENEYE_BUILD_PLAYER "Build unified runtime player" ON)
option(OPENEYE_BUILD_TESTS "Build API test app" ON)
option(OPENEYE_USE_LZ4 "Use LZ4 for asset/binary compression" ON)
option(OPENEYE_USE_ZSTD "Use Zstd for asset/binary compression" ON)
option(OPENEYE_USE_DILIGENT "Use Diligent Engine as RHI backend (requires thirdparty/DiligentCore)" OFF)

# Player build uses OpenGL only (no Diligent)
if(OPENEYE_BUILD_PLAYER)
    if(OPENEYE_USE_DILIGENT)
        set(OPENEYE_USE_DILIGENT OFF CACHE BOOL "Use Diligent (disabled for player; OpenGL is default)" FORCE)
        message(STATUS "Player build: Diligent disabled; OpenGL is the default backend")
    endif()
    set(OPENEYE_DILIGENT_AVAILABLE 0 CACHE INTERNAL "Diligent disabled for player build" FORCE)
endif()

# Global include directories (components will add their own)
# These are for legacy compatibility and third-party includes

# --------------------------------------
# LZ4 & Zstd (FetchContent)
# --------------------------------------
if(OPENEYE_USE_LZ4 OR OPENEYE_USE_ZSTD)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchCompression.cmake)
endif()

# --------------------------------------
# Diligent Engine (optional; requires thirdparty/DiligentCore + submodules)
# --------------------------------------
if(OPENEYE_USE_DILIGENT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDiligent.cmake)
endif()

# --------------------------------------
# GLAD 2 (header-only + single loader .c for MX context)
# --------------------------------------
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/thirdparty/glad/src/glad_gl_loader.c)
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
# Lua (Scripting engine)
# --------------------------------------
# Build Lua from local thirdparty source
set(LUA_SOURCES
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lapi.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lcode.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lctype.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/ldebug.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/ldo.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/ldump.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lfunc.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lgc.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/llex.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lmem.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lobject.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lopcodes.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lparser.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lstate.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lstring.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/ltable.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/ltm.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lundump.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lvm.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lzio.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lauxlib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lbaselib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lcorolib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/ldblib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/liolib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lmathlib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/loadlib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/loslib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lstrlib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/ltablib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/lutf8lib.c
    ${CMAKE_SOURCE_DIR}/thirdparty/lua/linit.c
)

add_library(Lua STATIC ${LUA_SOURCES})

target_include_directories(Lua
    PUBLIC
        ${CMAKE_SOURCE_DIR}/thirdparty/lua
)

# Disable warnings for Lua (third-party code)
if(MSVC)
    target_compile_options(Lua PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996)
    set_target_properties(Lua PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
endif()

# --------------------------------------
# OpenAL Soft (Audio backend)
# --------------------------------------
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "" FORCE)
set(ALSOFT_EMBED_HRTF_DATA OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/openal-soft)

# --------------------------------------
# Jolt Physics (thirdparty/JoltPhysics) - used by eyelib/physics/backend/jolt
# --------------------------------------
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Let parent control CXX flags" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Use parent warning level" FORCE)
set(USE_ASSERTS OFF CACHE BOOL "Disable Jolt asserts in all configs" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Disable LTO for Jolt" FORCE)
# Use dynamic CRT (MD/MDd) so Jolt links with rest of OpenEye (avoids LNK2038 in demo/tests).
# Per physics_advice checklist: Jolt and app must use same runtime and iterator debug level
# to avoid proxy corruption; dynamic CRT and same config (Debug/Release) keep these aligned.
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use dynamic MSVC runtime" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/JoltPhysics/Build ${CMAKE_BINARY_DIR}/thirdparty/JoltPhysics)

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
# Eyelib Engine Library (New Professional Structure)
# --------------------------------------
# Build eyelib alongside src for parallel development
option(OPENEYE_BUILD_EYELIB "Build new eyelib engine library" ON)

if(OPENEYE_BUILD_EYELIB)
    # Eyelib core module
    add_subdirectory(eyelib/core)
    add_subdirectory(api/core)
    # Eyelib math module (GLM-backed)
    add_subdirectory(eyelib/math)
    add_subdirectory(api/math)
    # Eyelib platform module (before assets so assets API can link it for ShaderWatcher/filesystem)
    add_subdirectory(eyelib/platform)
    add_subdirectory(api/platform)
    # Eyelib assets module
    add_subdirectory(eyelib/assets)
    add_subdirectory(api/assets)
    # Eyelib animation module
    add_subdirectory(eyelib/animation)
    add_subdirectory(api/animation)
    # Eyelib ECS module
    add_subdirectory(eyelib/ecs)
    add_subdirectory(api/ecs)
    # Eyelib scene module
    add_subdirectory(eyelib/scene)
    add_subdirectory(api/scene)
    # Eyelib serialize module
    add_subdirectory(eyelib/serialize)
    add_subdirectory(api/serialize)
    # Eyelib world mutation module
    add_subdirectory(eyelib/world)
    add_subdirectory(api/world)
    # Eyelib particles module
    add_subdirectory(eyelib/particles)
    add_subdirectory(api/particles)
    # Eyelib fluids module
    add_subdirectory(eyelib/fluids)
    add_subdirectory(api/fluids)
    # Eyelib audio module
    add_subdirectory(eyelib/audio)
    add_subdirectory(api/audio)
    # Eyelib debug module
    add_subdirectory(eyelib/debug)
    add_subdirectory(api/debug)
    # Eyelib physics module
    add_subdirectory(eyelib/physics)
    add_subdirectory(api/physics)
    # Eyelib render module
    add_subdirectory(eyelib/render)
    add_subdirectory(api/render)
    # Eyelib geometry module (mesh operations, selection, masking)
    add_subdirectory(eyelib/geometry)
    add_subdirectory(api/geometry)
    # Eyelib scripting module
    add_subdirectory(eyelib/scripting)
    add_subdirectory(api/scripting)
endif()

# Editor and game default OFF for now; set ON to build
set(OPENEYE_BUILD_EDITOR OFF CACHE BOOL "Build development editor")
set(OPENEYE_BUILD_GAME OFF CACHE BOOL "Build final game")

# --------------------------------------
# Editor Application (api client only)
# --------------------------------------
if(OPENEYE_BUILD_EDITOR)
    add_subdirectory(app/editor)
endif()

# --------------------------------------
# Game Application (Final Runtime)
# --------------------------------------
# Game application in app/openmeta/
# Game never depends on editor - clean separation
if(OPENEYE_BUILD_GAME)
    add_subdirectory(app/openmeta)
endif()

# --------------------------------------
# Unified Runtime Player (lib shared with Editor Game view)
# --------------------------------------
if(OPENEYE_BUILD_PLAYER OR OPENEYE_BUILD_EDITOR)
    add_subdirectory(app/player)
endif()

# --------------------------------------
# API Test Application
# --------------------------------------
if(OPENEYE_BUILD_TESTS)
    add_subdirectory(app/test)
endif()

# --------------------------------------
# Transform hierarchy benchmark (100k entities; requires EyelibWorld)
# --------------------------------------
add_subdirectory(app/benchmark_transform)

# --------------------------------------
# Physics determinism test (60 FPS vs 144 FPS → same result)
# --------------------------------------


# --------------------------------------
# Jolt physics demo (floor + sphere, raycast, AABB query)
# --------------------------------------
add_subdirectory(app/demo_physics_jolt)

# --------------------------------------
# Diligent standalone test (clear + present)
# --------------------------------------
if(OPENEYE_USE_DILIGENT)
    add_subdirectory(app/diligent_test)
endif()


