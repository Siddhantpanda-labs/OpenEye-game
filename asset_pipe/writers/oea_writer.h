#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace asset_pipe {

struct OeaInputEntry {
    uint32_t type = 0;
    std::string name;        // name inside package (e.g. "car.prefab")
    std::string sourcePath;  // source file to pack
};

class OeaWriter {
public:
    static bool Write(const std::string& packagePath, const std::vector<OeaInputEntry>& entries);
};

} // namespace asset_pipe

