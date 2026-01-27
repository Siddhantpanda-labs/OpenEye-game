# Eyelib Scripting Component

set(SCRIPTING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaVM.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaVM.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ScriptManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ScriptManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ScriptValue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/BindingHelpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/BindingHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/BindingRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/BindingRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/TimeBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/TimeBindings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/EntityBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/EntityBindings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/TransformBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/TransformBindings.h
)

add_library(EyelibScripting STATIC ${SCRIPTING_SOURCES})

target_include_directories(EyelibScripting
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibScripting
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibECS
        Lua
)
