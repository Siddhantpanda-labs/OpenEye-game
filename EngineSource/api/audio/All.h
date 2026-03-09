#pragma once

#include "AudioEffects.h"
#include "AudioClip.h"
#include "AudioDevice.h"
#include "AudioEvents.h"
#include "AudioListener.h"
#include "AudioMixer.h"
#include "AudioSource.h"

namespace eyelib {
namespace api {
namespace audio {
using ::eyelib::api::AudioDevice;
using ::eyelib::api::AudioClip;
using ::eyelib::api::AudioSource;
using ::eyelib::api::AudioListener;
using ::eyelib::api::AudioMixer;
using ::eyelib::api::AudioEvents;
using ::eyelib::api::Reverb;
using ::eyelib::api::Echo;
using ::eyelib::api::Filter;
using ::eyelib::api::Distortion;
using ::eyelib::api::ReverbSettings;
using ::eyelib::api::EchoSettings;
using ::eyelib::api::FilterSettings;
using ::eyelib::api::FilterType;
using ::eyelib::api::DistortionSettings;
} // namespace audio
} // namespace api
} // namespace eyelib
