# Eyelib World Mutation System API

set(API_WORLD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/World.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Observation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Observation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Transactions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Transactions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibWorldAPI STATIC ${API_WORLD_SOURCES})

target_include_directories(EyelibWorldAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/thirdparty
        ${CMAKE_SOURCE_DIR}/thirdparty/nlohmann
)

target_link_libraries(EyelibWorldAPI
    PUBLIC
        EyelibWorld
        EyelibCoreAPI
)
