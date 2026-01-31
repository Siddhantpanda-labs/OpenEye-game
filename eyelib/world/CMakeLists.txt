# Eyelib World Mutation System (core)

set(EYELIB_WORLD_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/World.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/World.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldState.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EntityId.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EntityRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EntityRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentStorage/IComponentStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ComponentStorage/ComponentStore.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldGraph/Hierarchy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldGraph/Hierarchy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldGraph/Metadata.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldGraph/Metadata.h
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldGraph/Tags.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldGraph/Tags.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/Command.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/CommandTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/CommandBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/CommandBuffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/CommandValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/CommandValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/CommandRecord.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Commands/CommandRecord.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Transactions/Transaction.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Transactions/Transaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Transactions/TransactionManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Transactions/TransactionManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Events/WorldEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Events/EventDispatcher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Events/EventDispatcher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization/WorldSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization/WorldSerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization/ComponentSerializer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization/ComponentSerializer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization/WorldSnapshot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Serialization/WorldSnapshot.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RegionQueries.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RegionQueries.h
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/RegionSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/systems/RegionSystem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/WorldScheduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/WorldScheduler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/observation/WorldObservation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/observation/WorldObservation.h
)

add_library(EyelibWorld STATIC ${EYELIB_WORLD_SOURCES})

target_include_directories(EyelibWorld
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/eyelib>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/thirdparty/nlohmann>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty
)

target_link_libraries(EyelibWorld
    PUBLIC
        EyelibCore
        EyelibMath
)
