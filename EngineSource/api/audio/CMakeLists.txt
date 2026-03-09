# Eyelib Audio API

set(API_AUDIO_SOURCES
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
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioEffects.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AudioEffects.h
    ${CMAKE_CURRENT_SOURCE_DIR}/All.h
)

add_library(EyelibAudioAPI STATIC ${API_AUDIO_SOURCES})

target_include_directories(EyelibAudioAPI
    PUBLIC
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}>
        $<BUILD_INTERFACE:${OPENEYE_ENGINE_SOURCE}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${OPENEYE_ENGINE_SOURCE}/eyelib
)

target_link_libraries(EyelibAudioAPI
    PUBLIC
        EyelibAudio
        EyelibCore
)
