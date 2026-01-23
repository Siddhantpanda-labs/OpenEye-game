# Eyelib Core API
# Client-facing API for core engine functionality

set(API_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Engine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Time.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibCoreAPI STATIC ${API_CORE_SOURCES})

target_include_directories(EyelibCoreAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # Allow includes like: "eyelib/core/Log.h"
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/api
)

# Link to implementation
target_link_libraries(EyelibCoreAPI
    PUBLIC
        EyelibCore
)
