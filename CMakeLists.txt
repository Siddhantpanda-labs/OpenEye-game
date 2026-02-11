cmake_minimum_required(VERSION 3.20)
project(OpenEye VERSION 0.1.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings
if(MSVC)
    # NOTE: Avoid /WX globally because some thirdparty deps (notably Bullet3Collision)
    # emit MSVC warnings like C4201, which then become hard build failures.
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
option(OPENEYE_USE_BGFX "Use bgfx as RHI backend (Phase 0: fetch and build)" ON)
option(OPENEYE_USE_DILIGENT "Use Diligent Engine as RHI backend (requires thirdparty/DiligentCore)" OFF)

# Global include directories (components will add their own)
# These are for legacy compatibility and third-party includes

# --------------------------------------
# LZ4 & Zstd (FetchContent)
# --------------------------------------
if(OPENEYE_USE_LZ4 OR OPENEYE_USE_ZSTD)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchCompression.cmake)
endif()

# --------------------------------------
# BGFX (FetchContent: bx, bimg, bgfx via pr0g/bgfx.cmake)
# --------------------------------------
if(OPENEYE_USE_BGFX)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchBGFX.cmake)
endif()

# --------------------------------------
# Diligent Engine (optional; requires thirdparty/DiligentCore + submodules)
# --------------------------------------
if(OPENEYE_USE_DILIGENT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDiligent.cmake)
endif()

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
# Bullet3 Physics Library
# --------------------------------------
# Configure Bullet3 build options
set(USE_GRAPHICAL_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(USE_DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(BULLET_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BULLET_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_GTEST OFF CACHE BOOL "" FORCE)
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

set(OPENEYE_BUILD_EDITOR OFF CACHE BOOL "Build development editor" FORCE)
set(OPENEYE_BUILD_GAME OFF CACHE BOOL "Build final game" FORCE)

# --------------------------------------
# Game Application (Final Runtime)
# --------------------------------------
# Game application in app/openmeta/
# Game never depends on editor - clean separation
if(OPENEYE_BUILD_GAME)
    add_subdirectory(app/openmeta)
endif()

# --------------------------------------
# Unified Runtime Player
# --------------------------------------
if(OPENEYE_BUILD_PLAYER)
    add_subdirectory(app/player)
endif()

# --------------------------------------
# API Test Application
# --------------------------------------
if(OPENEYE_BUILD_TESTS)
    add_subdirectory(app/test)
endif()

# --------------------------------------
# BGFX Phase 0 test (init + one frame)
# --------------------------------------
if(OPENEYE_USE_BGFX)
    add_subdirectory(app/bgfx_test)
endif()


