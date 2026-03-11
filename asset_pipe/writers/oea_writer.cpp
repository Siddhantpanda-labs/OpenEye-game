#include "asset_pipe/writers/oea_writer.h"
#include "asset_pipe/core/oea_format.h"

#include <cstdint>
#include <fstream>
#include <vector>

namespace asset_pipe {

namespace {

struct LoadedEntry {
    uint32_t type = 0;
    std::string name;
    std::vector<uint8_t> payload;
};

bool ReadAllBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff n = in.tellg();
    if (n < 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    if (!out.empty() && !in.read(reinterpret_cast<char*>(out.data()), n)) return false;
    return true;
}

} // namespace

bool OeaWriter::Write(const std::string& packagePath, const std::vector<OeaInputEntry>& entries) {
    using namespace OeaFormat;
    std::vector<LoadedEntry> loaded;
    loaded.reserve(entries.size());
    for (const auto& e : entries) {
        if (e.name.empty() || e.sourcePath.empty()) continue;
        LoadedEntry le;
        le.type = e.type;
        le.name = e.name;
        if (!ReadAllBytes(e.sourcePath, le.payload)) return false;
        loaded.push_back(std::move(le));
    }

    OeaHeader header;
    header.entryCount = static_cast<uint32_t>(loaded.size());

    uint64_t tocBytes = 0;
    for (const auto& e : loaded)
        tocBytes += static_cast<uint64_t>(sizeof(OeaEntryRecord) + e.name.size());

    uint64_t payloadOffset = static_cast<uint64_t>(sizeof(OeaHeader)) + tocBytes;
    std::vector<OeaEntryRecord> records;
    records.reserve(loaded.size());
    for (const auto& e : loaded) {
        OeaEntryRecord rec;
        rec.type = e.type;
        rec.nameLen = static_cast<uint32_t>(e.name.size());
        rec.offset = payloadOffset;
        rec.size = static_cast<uint64_t>(e.payload.size());
        payloadOffset += rec.size;
        records.push_back(rec);
    }

    std::ofstream out(packagePath, std::ios::binary);
    if (!out) return false;
    if (!out.write(reinterpret_cast<const char*>(&header), sizeof(header))) return false;
    for (size_t i = 0; i < loaded.size(); ++i) {
        if (!out.write(reinterpret_cast<const char*>(&records[i]), sizeof(records[i]))) return false;
        if (!loaded[i].name.empty() &&
            !out.write(loaded[i].name.data(), static_cast<std::streamsize>(loaded[i].name.size())))
            return false;
    }
    for (const auto& e : loaded) {
        if (!e.payload.empty() &&
            !out.write(reinterpret_cast<const char*>(e.payload.data()), static_cast<std::streamsize>(e.payload.size())))
            return false;
    }
    return out.good();
}

} // namespace asset_pipe

