# Eyelib Render API (minimal bring-up)

set(API_RENDER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Texture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Texture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Mesh.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Material.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Material.h
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RenderPipeline.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PostProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PostProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Sprite.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Sprite.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Text.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Text.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Light.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibRenderAPI STATIC ${API_RENDER_SOURCES})

target_include_directories(EyelibRenderAPI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/eyelib
        ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
)

target_link_libraries(EyelibRenderAPI
    PUBLIC
        EyelibRender
        EyelibMath
)

