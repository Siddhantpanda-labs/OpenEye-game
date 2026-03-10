#include "asset_pipe/writers/animation_writer.h"

#include <fstream>
#include <cstdint>

namespace asset_pipe {

namespace {

constexpr uint32_t kMagic = 0x414E4931u;  // "ANI1"
constexpr uint32_t kVersion = 1u;

} // namespace

bool AnimationWriter::Write(const ImportAnimation& animation, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    uint32_t magic = kMagic;
    uint32_t version = kVersion;
    uint32_t nameLen = static_cast<uint32_t>(animation.name.size());
    uint32_t channelCount = static_cast<uint32_t>(animation.channels.size());
    float duration = animation.duration;

    file.write(reinterpret_cast<const char*>(&magic), 4);
    file.write(reinterpret_cast<const char*>(&version), 4);
    file.write(reinterpret_cast<const char*>(&nameLen), 4);
    file.write(reinterpret_cast<const char*>(&channelCount), 4);
    file.write(reinterpret_cast<const char*>(&duration), 4);
    if (!animation.name.empty())
        file.write(animation.name.data(), animation.name.size());

    for (const auto& ch : animation.channels) {
        uint32_t boneNameLen = static_cast<uint32_t>(ch.boneName.size());
        int32_t boneIndex = ch.boneIndex;
        uint32_t posCount = static_cast<uint32_t>(ch.positionKeys.size());
        uint32_t rotCount = static_cast<uint32_t>(ch.rotationKeys.size());
        uint32_t sclCount = static_cast<uint32_t>(ch.scaleKeys.size());
        file.write(reinterpret_cast<const char*>(&boneNameLen), 4);
        file.write(reinterpret_cast<const char*>(&boneIndex), 4);
        file.write(reinterpret_cast<const char*>(&posCount), 4);
        file.write(reinterpret_cast<const char*>(&rotCount), 4);
        file.write(reinterpret_cast<const char*>(&sclCount), 4);
        if (boneNameLen) file.write(ch.boneName.data(), ch.boneName.size());
        for (const auto& pk : ch.positionKeys) {
            file.write(reinterpret_cast<const char*>(&pk.time), 4);
            file.write(reinterpret_cast<const char*>(&pk.value), 12);
        }
        for (const auto& rk : ch.rotationKeys) {
            file.write(reinterpret_cast<const char*>(&rk.time), 4);
            file.write(reinterpret_cast<const char*>(&rk.value), 16);
        }
        for (const auto& sk : ch.scaleKeys) {
            file.write(reinterpret_cast<const char*>(&sk.time), 4);
            file.write(reinterpret_cast<const char*>(&sk.value), 12);
        }
    }
    return file.good();
}

} // namespace asset_pipe
