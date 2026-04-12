# EyelibRendering — allowlisted copy from legacy layout (rhi, IBL, materials, textures, 2d, …).
# See README.md. No #include of legacy `eyelib/render/` path in sources.

set(_rendering_firewall_dir ${CMAKE_CURRENT_SOURCE_DIR})
file(GLOB_RECURSE _rendering_firewall_src CONFIGURE_DEPENDS
    "${_rendering_firewall_dir}/*.cpp"
    "${_rendering_firewall_dir}/*.c"
    "${_rendering_firewall_dir}/*.h"
    "${_rendering_firewall_dir}/*.hpp"
    "${_rendering_firewall_dir}/*.inl"
)
foreach(_rf_src ${_rendering_firewall_src})
    file(READ "${_rf_src}" _rf_content)
    string(FIND "${_rf_content}" "eyelib/render/" _rf_pos)
    if(NOT _rf_pos EQUAL -1)
        message(FATAL_ERROR
            "Rendering include firewall: '${_rf_src}' references legacy include path. See rendering/README.md.")
    endif()
endforeach()

file(GLOB_RECURSE RENDERING_CPP CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Vulkan-only module: never compile optional GL/Diligent RHI trees if present.
list(FILTER RENDERING_CPP EXCLUDE REGEX ".*/rhi/OpenGL/.*")
list(FILTER RENDERING_CPP EXCLUDE REGEX ".*/rhi/diligent/.*")

if(OPENEYE_USE_VULKAN)
    find_package(Vulkan QUIET)
endif()
if(NOT OPENEYE_USE_VULKAN OR NOT Vulkan_FOUND)
    list(FILTER RENDERING_CPP EXCLUDE REGEX ".*/rhi/Vulkan/.*")
endif()

add_library(EyelibRendering STATIC ${RENDERING_CPP})

target_include_directories(EyelibRendering
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glfw/include
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/glm
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/stb
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/assimp/include
)

target_link_libraries(EyelibRendering
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibPlatform
        EyelibDebug
    PRIVATE
        glfw
)

if(OPENEYE_USE_VULKAN AND Vulkan_FOUND)
    target_include_directories(EyelibRendering PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(EyelibRendering PRIVATE Vulkan::Vulkan)
    target_compile_definitions(EyelibRendering PRIVATE OPENEYE_VULKAN_AVAILABLE=1)
    get_filename_component(_OPENEYE_VULKAN_SDK_ROOT "${Vulkan_INCLUDE_DIRS}" DIRECTORY)
    if(WIN32)
        set(_OPENEYE_SHADERC_REL_LIB "${_OPENEYE_VULKAN_SDK_ROOT}/Lib/shaderc_combined.lib")
        set(_OPENEYE_SHADERC_DBG_LIB "${_OPENEYE_VULKAN_SDK_ROOT}/Lib/shaderc_combinedd.lib")
        if(EXISTS "${_OPENEYE_SHADERC_DBG_LIB}" AND EXISTS "${_OPENEYE_SHADERC_REL_LIB}")
            target_link_libraries(EyelibRendering PRIVATE
                "$<$<CONFIG:Debug>:${_OPENEYE_SHADERC_DBG_LIB}>"
                "$<$<NOT:$<CONFIG:Debug>>:${_OPENEYE_SHADERC_REL_LIB}>")
            target_compile_definitions(EyelibRendering PRIVATE OPENEYE_VULKAN_SHADERC=1)
        elseif(EXISTS "${_OPENEYE_SHADERC_REL_LIB}")
            target_link_libraries(EyelibRendering PRIVATE "${_OPENEYE_SHADERC_REL_LIB}")
            target_compile_definitions(EyelibRendering PRIVATE OPENEYE_VULKAN_SHADERC=1)
        endif()
    else()
        find_library(_OPENEYE_SHADERC_LIB shaderc_combined PATHS "${_OPENEYE_VULKAN_SDK_ROOT}/lib" NO_DEFAULT_PATH)
        if(_OPENEYE_SHADERC_LIB)
            target_link_libraries(EyelibRendering PRIVATE "${_OPENEYE_SHADERC_LIB}")
            target_compile_definitions(EyelibRendering PRIVATE OPENEYE_VULKAN_SHADERC=1)
        endif()
    endif()
    if(OPENEYE_VULKAN_DEBUG_CLEAR_RED)
        target_compile_definitions(EyelibRendering PRIVATE OPENEYE_VULKAN_DEBUG_CLEAR_RED=1)
    endif()
    if(OPENEYE_VULKAN_VALIDATION)
        target_compile_definitions(EyelibRendering PRIVATE OPENEYE_VULKAN_VALIDATION=1)
    endif()
endif()

if(WIN32)
    target_link_libraries(EyelibRendering PRIVATE ${OPENEYE_ENGINE_SOURCE}/thirdparty/assimp/lib/assimp-vc143-mt.lib)
endif()
