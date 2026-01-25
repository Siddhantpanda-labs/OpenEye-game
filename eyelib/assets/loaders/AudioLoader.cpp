#include "AudioLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/AudioClip.h"
#include "eyelib/core/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <thread>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

#include "stb/stb_vorbis.c"

namespace eyelib {
namespace assets {

namespace {
std::string Lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool LoadWav(const std::string& path, AudioClip& clip) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    auto readU32 = [&]() {
        uint32_t v = 0;
        file.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    };
    auto readU16 = [&]() {
        uint16_t v = 0;
        file.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    };

    char riff[4];
    file.read(riff, 4);
    if (std::string(riff, 4) != "RIFF") return false;
    (void)readU32(); // file size
    char wave[4];
    file.read(wave, 4);
    if (std::string(wave, 4) != "WAVE") return false;

    uint16_t audioFormat = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    std::vector<uint8_t> data;

    while (file && !file.eof()) {
        char chunkId[4];
        file.read(chunkId, 4);
        if (file.gcount() < 4) break;
        uint32_t chunkSize = readU32();
        std::string id(chunkId, 4);
        if (id == "fmt ") {
            audioFormat = readU16();
            channels = readU16();
            sampleRate = readU32();
            (void)readU32(); // byte rate
            (void)readU16(); // block align
            bitsPerSample = readU16();
            file.seekg(static_cast<std::streamoff>(chunkSize) - 16, std::ios::cur);
        } else if (id == "data") {
            data.resize(chunkSize);
            file.read(reinterpret_cast<char*>(data.data()), chunkSize);
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (data.empty() || channels == 0 || sampleRate == 0) return false;

    clip.channels = static_cast<int>(channels);
    clip.sampleRate = static_cast<int>(sampleRate);

    if (audioFormat == 1 && bitsPerSample == 16) {
        const int16_t* samples = reinterpret_cast<const int16_t*>(data.data());
        size_t count = data.size() / sizeof(int16_t);
        clip.samples.resize(count);
        for (size_t i = 0; i < count; ++i) {
            clip.samples[i] = static_cast<float>(samples[i]) / 32768.0f;
        }
        return true;
    }

    if (audioFormat == 3 && bitsPerSample == 32) {
        const float* samples = reinterpret_cast<const float*>(data.data());
        size_t count = data.size() / sizeof(float);
        clip.samples.assign(samples, samples + count);
        return true;
    }

    return false;
}

bool LoadOgg(const std::string& path, AudioClip& clip) {
    int channels = 0;
    int sampleRate = 0;
    short* output = nullptr;
    int samples = stb_vorbis_decode_filename(path.c_str(), &channels, &sampleRate, &output);
    if (samples <= 0 || !output) return false;
    clip.channels = channels;
    clip.sampleRate = sampleRate;
    clip.samples.resize(static_cast<size_t>(samples) * channels);
    for (int i = 0; i < samples * channels; ++i) {
        clip.samples[i] = static_cast<float>(output[i]) / 32768.0f;
    }
    free(output);
    return true;
}

bool LoadMp3(const std::string& path, AudioClip& clip) {
#if defined(_WIN32)
    using Microsoft::WRL::ComPtr;
    auto toWide = [](const std::string& s) {
        if (s.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (size <= 0) return std::wstring();
        std::wstring out(size - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), size);
        return out;
    };

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInit = SUCCEEDED(hr);
    hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        if (comInit) CoUninitialize();
        return false;
    }

    ComPtr<IMFSourceReader> reader;
    std::wstring wpath = toWide(path);
    hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader);
    if (FAILED(hr)) {
        MFShutdown();
        if (comInit) CoUninitialize();
        return false;
    }

    ComPtr<IMFMediaType> type;
    hr = MFCreateMediaType(&type);
    if (FAILED(hr)) {
        MFShutdown();
        if (comInit) CoUninitialize();
        return false;
    }

    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 0);
    type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 0);

    hr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get());
    if (FAILED(hr)) {
        MFShutdown();
        if (comInit) CoUninitialize();
        return false;
    }

    ComPtr<IMFMediaType> actual;
    hr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actual);
    if (FAILED(hr)) {
        MFShutdown();
        if (comInit) CoUninitialize();
        return false;
    }

    UINT32 channels = 0;
    UINT32 sampleRate = 0;
    UINT32 bitsPerSample = 0;
    actual->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    actual->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    actual->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
    if (channels == 0 || sampleRate == 0 || bitsPerSample != 16) {
        MFShutdown();
        if (comInit) CoUninitialize();
        return false;
    }

    std::vector<float> samples;
    while (true) {
        DWORD flags = 0;
        ComPtr<IMFSample> sample;
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;

        BYTE* data = nullptr;
        DWORD maxLen = 0;
        DWORD curLen = 0;
        if (FAILED(buffer->Lock(&data, &maxLen, &curLen))) continue;

        const size_t count = curLen / sizeof(int16_t);
        const int16_t* pcm = reinterpret_cast<const int16_t*>(data);
        samples.reserve(samples.size() + count);
        for (size_t i = 0; i < count; ++i) {
            samples.push_back(static_cast<float>(pcm[i]) / 32768.0f);
        }
        buffer->Unlock();
    }

    MFShutdown();
    if (comInit) CoUninitialize();

    if (samples.empty()) return false;
    clip.channels = static_cast<int>(channels);
    clip.sampleRate = static_cast<int>(sampleRate);
    clip.samples = std::move(samples);
    return true;
#else
    (void)path;
    (void)clip;
    return false;
#endif
}
} // namespace

AssetId AudioLoader::LoadAudio(const std::string& path) {
    auto& manager = AssetManager::Get();
    const AssetId id = manager.Registry().RegisterAsset(path, "Audio");

    AudioClip clip;
    const std::string lower = Lower(path);
    bool ok = false;
    if (lower.rfind(".wav") != std::string::npos) {
        ok = LoadWav(path, clip);
    } else if (lower.rfind(".ogg") != std::string::npos) {
        ok = LoadOgg(path, clip);
    } else if (lower.rfind(".mp3") != std::string::npos) {
        ok = LoadMp3(path, clip);
        if (!ok) core::Log::Warn("AudioLoader: MP3 load failed.");
    }

    if (ok) {
        manager.Cache().Store(id, std::make_shared<AudioClip>(std::move(clip)), "Audio");
    }
    return id;
}

void AudioLoader::LoadAudioAsync(const std::string& path, LoadCallback callback) {
    std::thread([path, callback]() {
        const AssetId id = LoadAudio(path);
        if (callback) callback(id);
    }).detach();
}

} // namespace assets
} // namespace eyelib

