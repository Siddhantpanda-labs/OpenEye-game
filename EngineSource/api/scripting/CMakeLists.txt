# Eyelib Scripting API
# Public API with PIMPL pattern for ABI stability

set(SCRIPTING_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Script.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Script.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ScriptVM.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ScriptVM.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibScriptingAPI STATIC ${SCRIPTING_API_SOURCES})

target_include_directories(EyelibScriptingAPI
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibScriptingAPI
    PUBLIC
        EyelibScripting
)
