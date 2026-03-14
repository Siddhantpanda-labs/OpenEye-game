# OpenEye Engine Source (thirdparty, eyelib, api, app)
# Paths use CMAKE_CURRENT_SOURCE_DIR = EngineSource/
set(OPENEYE_ENGINE_SOURCE ${CMAKE_CURRENT_SOURCE_DIR})

# Vulkan: ON = build Vulkan RHI and ImGui Vulkan backend when SDK is found (editor/player default backend).
option(OPENEYE_USE_VULKAN "Build with Vulkan support (RHI + ImGui backend; requires Vulkan SDK)" ON)

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
add_library(glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glad/src/glad_gl_loader.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glad/include)

# --------------------------------------
# QR code generator (Nayuki) - for VR server pairing overlay
# --------------------------------------
add_library(qrcodegen STATIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/qrcodegen/qrcodegen.cpp)
target_include_directories(qrcodegen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/qrcodegen)
if(MSVC)
    target_compile_options(qrcodegen PRIVATE /W3 /wd4244 /wd4267)
endif()

# --------------------------------------
# GLFW (use GLFW's own CMake build system - proper way)
# --------------------------------------
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)

# --------------------------------------
# Lua (Scripting engine)
# --------------------------------------
set(LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lapi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lcode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lctype.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/ldebug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/ldo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/ldump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lfunc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lgc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/llex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lobject.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lopcodes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lparser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lstate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lstring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/ltable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/ltm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lundump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lvm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lzio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lauxlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lbaselib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lcorolib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/ldblib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/liolib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lmathlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/loadlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/loslib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lstrlib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/ltablib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/lutf8lib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua/linit.c
)

add_library(Lua STATIC ${LUA_SOURCES})

target_include_directories(Lua
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua
)

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
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/openal-soft)

# --------------------------------------
# Jolt Physics (thirdparty/JoltPhysics) - used by eyelib/physics/backend/jolt
# --------------------------------------
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Let parent control CXX flags" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Use parent warning level" FORCE)
set(USE_ASSERTS OFF CACHE BOOL "Disable Jolt asserts in all configs" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Disable LTO for Jolt" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use dynamic MSVC runtime" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/JoltPhysics/Build ${CMAKE_BINARY_DIR}/thirdparty/JoltPhysics)

# --------------------------------------
# OpenXR SDK (optional; for VR provider)
# --------------------------------------
if(OPENEYE_USE_OPENXR)
    set(BUILD_TESTS OFF CACHE BOOL "Build OpenXR tests" FORCE)
    set(BUILD_API_LAYERS OFF CACHE BOOL "Build OpenXR API layers" FORCE)
    set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_SDK_TESTS OFF CACHE BOOL "Build OpenXR SDK samples" FORCE)
    set(BUILD_LOADER ON CACHE BOOL "Build OpenXR loader" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/OpenXR-SDK)
endif()

# Vulkan (for ImGui Vulkan backend; only when OPENEYE_USE_VULKAN and SDK found)
if(OPENEYE_USE_VULKAN)
    find_package(Vulkan QUIET)
    if(NOT Vulkan_FOUND)
        message(STATUS "OPENEYE_USE_VULKAN=ON but Vulkan SDK not found; ImGui Vulkan backend disabled. Set VULKAN_SDK or install SDK to enable.")
    endif()
endif()

# ImGui library (shared across editor and game)
set(IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)
if(Vulkan_FOUND)
    list(APPEND IMGUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_vulkan.cpp)
endif()

set(IMGUIZMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuizmo/ImGuizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuizmo/ImCurveEdit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuizmo/ImGradient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuizmo/ImSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuizmo/GraphEditor.cpp
)

add_library(OpenEyeImGui STATIC
    ${IMGUI_SOURCES}
    ${IMGUIZMO_SOURCES}
)

target_include_directories(OpenEyeImGui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuizmo
)

target_link_libraries(OpenEyeImGui
    PUBLIC
        glad
        glfw
)
if(Vulkan_FOUND)
    target_link_libraries(OpenEyeImGui PUBLIC Vulkan::Vulkan)
    target_compile_definitions(OpenEyeImGui PUBLIC OPENEYE_IMGUI_VULKAN=1)
endif()

# --------------------------------------
# Eyelib Engine Library (New Professional Structure)
# --------------------------------------
option(OPENEYE_BUILD_EYELIB "Build new eyelib engine library" ON)

if(OPENEYE_BUILD_EYELIB)
    add_subdirectory(eyelib/core)
    add_subdirectory(api/core)
    add_subdirectory(eyelib/math)
    add_subdirectory(api/math)
    add_subdirectory(eyelib/platform)
    add_subdirectory(api/platform)
    add_subdirectory(eyelib/asset)
    add_subdirectory(api/asset)
    add_subdirectory(eyelib/animation)
    add_subdirectory(api/animation)
    add_subdirectory(eyelib/scene)
    add_subdirectory(api/scene)
    add_subdirectory(eyelib/serialize)
    add_subdirectory(api/serialize)
    add_subdirectory(eyelib/world)
    add_subdirectory(api/world)
    add_subdirectory(eyelib/particles)
    add_subdirectory(api/particles)
    add_subdirectory(eyelib/fluids)
    add_subdirectory(api/fluids)
    add_subdirectory(eyelib/audio)
    add_subdirectory(api/audio)
    add_subdirectory(eyelib/debug)
    add_subdirectory(api/debug)
    add_subdirectory(eyelib/physics)
    add_subdirectory(api/physics)
    add_subdirectory(eyelib/render)
    add_subdirectory(api/render)
    add_subdirectory(eyelib/geometry)
    add_subdirectory(api/geometry)
    add_subdirectory(eyelib/scripting)
    add_subdirectory(api/scripting)
    add_subdirectory(eyelib/vr)
endif()

# Editor: use root option (default ON). Game: still OFF by default.
set(OPENEYE_BUILD_GAME OFF CACHE BOOL "Build final game")

# Vulkan shaders: compile with EngineSource/tools/compile_vulkan_shaders.py (glslc); .spv live next to sources.

# --------------------------------------
# Copy engine assets (shaders, default materials, etc.) to Engine/assets for runtime
# --------------------------------------
add_custom_target(CopyEngineAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/Engine/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/Engine/assets
    COMMENT "Copying engine assets to Engine/assets"
)

# --------------------------------------
# Editor Application (api client only)
# --------------------------------------
if(OPENEYE_BUILD_EDITOR)
    add_subdirectory(app/editor)
    add_dependencies(OpenEyeEditor CopyEngineAssets)
endif()

# --------------------------------------
# Game Application (Final Runtime)
# --------------------------------------
if(OPENEYE_BUILD_GAME)
    add_subdirectory(app/openmeta)
endif()

# --------------------------------------
# Tienet (H.264 encoder: MF on Windows, optional NVENC)
# --------------------------------------
if(OPENEYE_BUILD_STREAMING AND OPENEYE_STREAMING_H264)
    set(TIENET_ENABLE_NVENC ${OPENEYE_STREAMING_NVENC} CACHE BOOL "Use NVENC in Tienet" FORCE)
    set(OPENEYE_VIDEO_CODEC_SDK_PATH "c:/Video_Codec_SDK_13.0.19" CACHE PATH "Path to NVIDIA Video Codec SDK (e.g. c:/Video_Codec_SDK_13.0.19)")
    if(OPENEYE_VIDEO_CODEC_SDK_PATH)
        set(ENV{VIDEO_CODEC_SDK_DIR} "${OPENEYE_VIDEO_CODEC_SDK_PATH}")
    endif()
    add_subdirectory(eyelib/Tienet)
endif()

# --------------------------------------
# Unified Runtime Player (lib shared with Editor Game view)
# --------------------------------------
if(OPENEYE_BUILD_STREAMING)
    add_subdirectory(app/streaming)
endif()

if(OPENEYE_BUILD_PLAYER OR OPENEYE_BUILD_EDITOR)
    add_subdirectory(app/player)
    add_dependencies(OpenEyePlayer CopyEngineAssets)
endif()

# --------------------------------------
# API Test Application
# --------------------------------------
if(OPENEYE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/test")
    add_subdirectory(app/test)
endif()

# --------------------------------------
# Canonical viewer (loads asset .mesh, displays with default material)
# --------------------------------------
add_subdirectory(app/canonical_viewer)

# --------------------------------------
# Transform hierarchy benchmark (100k entities; requires EyelibWorld)
# --------------------------------------

# --------------------------------------
# Windows VR streaming test client (same protocol as Flutter; GLFW + MF decode)
# --------------------------------------
if(WIN32)
    add_subdirectory(app/metaworld/client/win_client)
endif()

# --------------------------------------
# Jolt physics demo (floor + sphere, raycast, AABB query)
# --------------------------------------

# --------------------------------------
# Diligent standalone test (clear + present)
# --------------------------------------
if(OPENEYE_USE_DILIGENT)
endif()
