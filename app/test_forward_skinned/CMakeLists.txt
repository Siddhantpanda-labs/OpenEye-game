# Test: Forward path + skinned model, OpenGL backend only.
# Same pipeline flow as the player; minimal app (no overlay, physics, scripting).
# Only built when the player is built (same libs).

if(NOT OPENEYE_BUILD_PLAYER)
    return()
endif()

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_SOURCE_DIR}/app/player/AssetBinding.cpp
    ${CMAKE_SOURCE_DIR}/app/player/RenderFrameSync.cpp
    ${CMAKE_SOURCE_DIR}/app/player/WorldRender.cpp
    ${CMAKE_SOURCE_DIR}/app/player/WorldAnimation.cpp
)

add_executable(TestForwardSkinned ${TEST_SOURCES})

target_include_directories(TestForwardSkinned
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/api
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
        ${CMAKE_SOURCE_DIR}/app/player
)

target_link_libraries(TestForwardSkinned
    PRIVATE
        EyelibCoreAPI
        EyelibPlatformAPI
        EyelibAssetsAPI
        EyelibWorldAPI
        EyelibRenderAPI
        EyelibAnimationAPI
)

# Run from project root so scenes/test_forward_skinned.json and assets/ are found,
# or run from build/bin/Debug after copying scenes/ and assets/ there.
