# Eyelib Audio Module (backend-agnostic placeholder)

set(AUDIO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioDevice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioDevice.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioClip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioClip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioSource.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioSource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioListener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioListener.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioMixer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioMixer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioEvents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioEvents.h
    ${CMAKE_CURRENT_SOURCE_DIR}/effects/Reverb.h
    ${CMAKE_CURRENT_SOURCE_DIR}/effects/Echo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/effects/Filter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/effects/Distortion.h
)

add_library(EyelibAudio STATIC ${AUDIO_SOURCES})

target_include_directories(EyelibAudio
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/eyelib>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/thirdparty/openal-soft/include>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/thirdparty>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(EyelibAudio
    PUBLIC
        EyelibCore
        EyelibMath
        EyelibAssets
        OpenAL
)

if(WIN32)
    target_link_libraries(EyelibAudio PRIVATE mfplat mfreadwrite mfuuid)
endif()
