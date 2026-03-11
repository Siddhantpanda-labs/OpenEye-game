#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace asset_pipe {

struct OeaInputEntry {
    uint32_t type = 0;
    std::string name;        // name inside package (e.g. "car.prefab")
    std::string sourcePath;  // source file to pack
    std::vector<uint8_t> payload; // optional in-memory payload; preferred when non-empty
};

class OeaWriter {
public:
    static bool Write(const std::string& packagePath, const std::vector<OeaInputEntry>& entries);
};

} // namespace asset_pipe

