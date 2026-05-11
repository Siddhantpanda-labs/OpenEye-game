# OpenEye AI Playground — api/ client: live scene + chat shell (LLM/tools in later phases).

set(AI_PLAYGROUND_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AIPlaygroundApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AIPlaygroundUI.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldToolBridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlaygroundWorldObservation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlaygroundOpenAIHttp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlaygroundOpenAiCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlaygroundGeminiCoordinator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PlaygroundProject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../player/PlayerConfig.cpp
)

add_executable(OpenEyeAIPlayground ${AI_PLAYGROUND_SOURCES})

target_include_directories(OpenEyeAIPlayground
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}
        ${OPENEYE_ENGINE_SOURCE}/api
        ${OPENEYE_ENGINE_SOURCE}/thirdparty/nlohmann
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../player
)

target_link_libraries(OpenEyeAIPlayground
    PRIVATE
        OpenEyeGameRuntime
)

if(WIN32)
    target_link_libraries(OpenEyeAIPlayground PRIVATE winhttp)
endif()

if(MSVC)
    target_compile_options(OpenEyeAIPlayground PRIVATE /wd4100)
    # M4.8 — same dual-stack stb_image / VMA dedup window as OpenEyePlayer + OpenEyeEditor.
    # AIPlayground links OpenEyeGameRuntime which still pulls EyelibRenderAPI (legacy) +
    # EyelibRenderingAPI (new) for the dual-stack window. Static-lib link propagation puts
    # both Texture2D.obj copies on the link line. Drop with M5 / legacy decommission.
    target_link_options(OpenEyeAIPlayground PRIVATE /FORCE:MULTIPLE)
endif()
