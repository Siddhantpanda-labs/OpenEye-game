# Eyelib Scripting API

# Phase 1: Minimal API (just header for now)
# Full API implementation will come in Phase 2

set(API_SCRIPTING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibScriptingAPI INTERFACE)

target_include_directories(EyelibScriptingAPI
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibScriptingAPI
    INTERFACE
        EyelibScripting
)
