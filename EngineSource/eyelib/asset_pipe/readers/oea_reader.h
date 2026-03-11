#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace asset_pipe {

struct OeaEntry {
    uint32_t type = 0;
    std::string name;
    uint32_t flags = 0;
    uint64_t offset = 0;
    uint64_t size = 0;
    uint64_t uncompressedSize = 0;
};

class OeaReader {
public:
    static bool ReadIndex(const std::string& packagePath, std::vector<OeaEntry>& outEntries);
    static bool ExtractAll(const std::string& packagePath, const std::string& outDir, std::vector<OeaEntry>* outEntries = nullptr);
    static bool ReadEntryData(const std::string& packagePath, const std::string& entryName, std::vector<uint8_t>& outData);
};

} // namespace asset_pipe

