# OpenEye API test app

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

add_executable(OpenEyeApiTests ${TEST_SOURCES})

target_include_directories(OpenEyeApiTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/api
)

target_link_libraries(OpenEyeApiTests
    PRIVATE
        EyelibCoreAPI
        EyelibMathAPI
        EyelibPlatformAPI
        EyelibRenderAPI
        EyelibPhysicsAPI
        EyelibAnimationAPI
        EyelibECSAPI
        EyelibSceneAPI
        EyelibAssetsAPI
        EyelibSerializeAPI
        EyelibParticlesAPI
        EyelibFluidsAPI
        EyelibAudioAPI
        EyelibDebugAPI
        EyelibGeometryAPI
)
