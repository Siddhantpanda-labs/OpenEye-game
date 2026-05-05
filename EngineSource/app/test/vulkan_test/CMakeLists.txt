# Vulkan test: fullscreen image via RHI + Vulkan, using passes/post/copy and assets/textures/checke.png
set(VULKAN_TEST_SOURCES main.cpp)
add_executable(VulkanTest ${VULKAN_TEST_SOURCES})

target_include_directories(VulkanTest PRIVATE
    ${OPENEYE_ENGINE_SOURCE}
    ${OPENEYE_ENGINE_SOURCE}/api
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(VulkanTest PRIVATE
    EyelibCoreAPI
    EyelibPlatformAPI
    EyelibRenderAPI
)
add_dependencies(VulkanTest CopyEngineAssets)
