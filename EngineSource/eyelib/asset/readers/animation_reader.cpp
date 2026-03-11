#include "asset/readers/animation_reader.h"

#include <fstream>
#include <cstdint>

namespace asset {

namespace {

constexpr uint32_t kMagic = 0x414E4931u;  // ANI1

bool ReadU32(std::istream& f, uint32_t& out) {
    return !!f.read(reinterpret_cast<char*>(&out), 4);
}
bool ReadI32(std::istream& f, int32_t& out) {
    return !!f.read(reinterpret_cast<char*>(&out), 4);
}
bool ReadF32(std::istream& f, float& out) {
    return !!f.read(reinterpret_cast<char*>(&out), 4);
}

} // namespace

bool AnimationReader::Read(const std::string& path, ImportAnimation& out) {
    out = ImportAnimation{};
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0, nameLen = 0, channelCount = 0;
    if (!ReadU32(f, magic) || magic != kMagic) return false;
    if (!ReadU32(f, version) || version != 1) return false;
    if (!ReadU32(f, nameLen)) return false;
    if (!ReadU32(f, channelCount)) return false;
    if (!ReadF32(f, out.duration)) return false;
    if (nameLen > 0) {
        out.name.resize(nameLen);
        if (!f.read(&out.name[0], nameLen)) return false;
    }
    out.channels.resize(channelCount);
    for (uint32_t c = 0; c < channelCount; ++c) {
        AnimationChannel& ch = out.channels[c];
        uint32_t boneNameLen = 0;
        if (!ReadU32(f, boneNameLen)) return false;
        if (!ReadI32(f, ch.boneIndex)) return false;
        uint32_t posCount = 0, rotCount = 0, sclCount = 0;
        if (!ReadU32(f, posCount) || !ReadU32(f, rotCount) || !ReadU32(f, sclCount)) return false;
        if (boneNameLen > 0) {
            ch.boneName.resize(boneNameLen);
            if (!f.read(&ch.boneName[0], boneNameLen)) return false;
        }
        ch.positionKeys.resize(posCount);
        for (auto& pk : ch.positionKeys) {
            if (!ReadF32(f, pk.time) || !f.read(reinterpret_cast<char*>(&pk.value), 12)) return false;
        }
        ch.rotationKeys.resize(rotCount);
        for (auto& rk : ch.rotationKeys) {
            if (!ReadF32(f, rk.time) || !f.read(reinterpret_cast<char*>(&rk.value), 16)) return false;
        }
        ch.scaleKeys.resize(sclCount);
        for (auto& sk : ch.scaleKeys) {
            if (!ReadF32(f, sk.time) || !f.read(reinterpret_cast<char*>(&sk.value), 12)) return false;
        }
    }
    return true;
}

} // namespace asset
