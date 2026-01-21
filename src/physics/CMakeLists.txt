# OpenEye Physics Component
# Physics systems: PhysicsWorld, CollisionShapes, RigidBodies, CharacterController, Queries

set(PHYSICS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysicsWorld.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shapes/CollisionShapeFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shapes/CollisionShapeFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bodies/RigidBodyFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bodies/RigidBodyFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers/CharacterControllerFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/controllers/CharacterControllerFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/SoftBodyFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/SoftBodyFactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/templates/ClothTemplates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/softbody/templates/ClothTemplates.h
    ${CMAKE_CURRENT_SOURCE_DIR}/queries/PhysicsQuery.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/queries/PhysicsQuery.h
    ${CMAKE_CURRENT_SOURCE_DIR}/queries/PhysicsQueryTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/PhysicsUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/PhysicsUtils.h
)

# API sources
set(PHYSICS_API_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/api/PhysicsWorld.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/api/PhysicsWorld.h
)

add_library(OpenEyePhysics STATIC ${PHYSICS_SOURCES} ${PHYSICS_API_SOURCES})

target_include_directories(OpenEyePhysics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
        # API folder for apps to use
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include/physics/api>
    PRIVATE
        # Third-party includes - internal only
        ${CMAKE_SOURCE_DIR}/thirdparty/bullet3/src
        ${CMAKE_SOURCE_DIR}/thirdparty/glm
)

# Suppress Bullet3 warnings for physics files
if(MSVC)
    set_source_files_properties(
        ${PHYSICS_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "/wd4127 /wd4456"
    )
endif()

# Dependencies
target_link_libraries(OpenEyePhysics
    PUBLIC
        OpenEyeCore
        OpenEyeRendering
        BulletDynamics
        BulletCollision
        BulletSoftBody
        LinearMath
)

# Suppress warnings for Bullet3 targets
if(MSVC)
    if(TARGET BulletDynamics)
        target_compile_options(BulletDynamics PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletDynamics PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET BulletCollision)
        target_compile_options(BulletCollision PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(BulletCollision PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
    if(TARGET LinearMath)
        target_compile_options(LinearMath PRIVATE /W3 /wd4244 /wd4267 /wd4100 /wd4996 /wd4127 /wd4456)
        set_target_properties(LinearMath PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
endif()

