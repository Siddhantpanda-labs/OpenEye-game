# Eyelib Core Component
# Base systems: Engine, Time, Log

set(CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/GLDrawDebugLabel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GLDrawDebugLabel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator/LinearAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator/PoolAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator/FreeListAllocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/job/ThreadPool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/job/TaskGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin/PluginManager.cpp
)

add_library(EyelibCore STATIC ${CORE_SOURCES})

target_include_directories(EyelibCore
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # Third-party includes - internal only
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
)

# Link GLFW for time system (glfwGetTime)
target_link_libraries(EyelibCore
    PRIVATE
        glfw
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(EyelibCore
        PRIVATE
            opengl32
    )
endif()
