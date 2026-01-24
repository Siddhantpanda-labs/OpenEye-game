#pragma once

#include "eyelib/scene/Scene.h"

#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace serialize {

enum class BinaryCompression {
    None = 0,
    RLE = 1,
    LZ4 = 2,
    Zstd = 3
};

class BinarySerializer {
public:
    static std::vector<uint8_t> SerializeScene(const scene::Scene& scene, BinaryCompression compression = BinaryCompression::None);
    static bool DeserializeScene(scene::Scene& scene, const std::vector<uint8_t>& data);
    static bool SaveToFile(const scene::Scene& scene, const std::string& path, BinaryCompression compression = BinaryCompression::None);
    static bool LoadFromFile(scene::Scene& scene, const std::string& path);
};

} // namespace serialize
} // namespace eyelib
