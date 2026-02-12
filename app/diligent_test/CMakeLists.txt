# Standalone Diligent test: window + device + clear swap chain + present.
# Only built when Diligent backend is available.
if(NOT OPENEYE_USE_DILIGENT)
    return()
endif()

add_executable(OpenEyeDiligentTest ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)

target_include_directories(OpenEyeDiligentTest
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/eyelib
)

target_link_libraries(OpenEyeDiligentTest
    PRIVATE
        EyelibCore
        EyelibMath
        EyelibPlatform
        EyelibRender
        EyelibDebug
)

# Copy test shaders to output dir so exe finds assets/shaders/diligent/test/solid.vsh when run from build/bin/Debug
add_custom_command(TARGET OpenEyeDiligentTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/shaders/diligent/test
        $<TARGET_FILE_DIR:OpenEyeDiligentTest>/assets/shaders/diligent/test
    COMMENT "Copy diligent test shaders to output"
)
