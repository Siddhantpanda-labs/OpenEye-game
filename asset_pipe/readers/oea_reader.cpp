#include "asset_pipe/readers/oea_reader.h"
#include "asset_pipe/core/oea_format.h"

#include <filesystem>
#include <fstream>

namespace asset_pipe {

namespace {
namespace fs = std::filesystem;

bool ReadHeaderAndIndex(std::ifstream& in, std::vector<OeaEntry>& outEntries) {
    using namespace OeaFormat;
    OeaHeader header{};
    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
    if (header.magic != kMagic || header.version != kVersion) return false;

    outEntries.clear();
    outEntries.reserve(header.entryCount);
    for (uint32_t i = 0; i < header.entryCount; ++i) {
        OeaEntryRecord rec{};
        if (!in.read(reinterpret_cast<char*>(&rec), sizeof(rec))) return false;
        OeaEntry e;
        e.type = rec.type;
        e.offset = rec.offset;
        e.size = rec.size;
        if (rec.nameLen > 0) {
            e.name.resize(rec.nameLen);
            if (!in.read(&e.name[0], rec.nameLen)) return false;
        }
        outEntries.push_back(std::move(e));
    }
    return true;
}

} // namespace

bool OeaReader::ReadIndex(const std::string& packagePath, std::vector<OeaEntry>& outEntries) {
    std::ifstream in(packagePath, std::ios::binary);
    if (!in) return false;
    return ReadHeaderAndIndex(in, outEntries);
}

bool OeaReader::ExtractAll(const std::string& packagePath, const std::string& outDir, std::vector<OeaEntry>* outEntries) {
    std::ifstream in(packagePath, std::ios::binary);
    if (!in) return false;
    std::vector<OeaEntry> entries;
    if (!ReadHeaderAndIndex(in, entries)) return false;

    fs::create_directories(outDir);
    for (const auto& e : entries) {
        if (e.name.empty()) continue;
        const fs::path p = fs::path(outDir) / e.name;
        if (p.has_parent_path()) fs::create_directories(p.parent_path());

        std::ofstream out(p.string(), std::ios::binary);
        if (!out) return false;
        in.seekg(static_cast<std::streamoff>(e.offset), std::ios::beg);
        if (!in) return false;

        constexpr size_t kChunk = 64 * 1024;
        std::vector<char> buffer(kChunk);
        uint64_t remaining = e.size;
        while (remaining > 0) {
            const size_t n = static_cast<size_t>(remaining > kChunk ? kChunk : remaining);
            if (!in.read(buffer.data(), static_cast<std::streamsize>(n))) return false;
            if (!out.write(buffer.data(), static_cast<std::streamsize>(n))) return false;
            remaining -= static_cast<uint64_t>(n);
        }
    }
    if (outEntries) *outEntries = std::move(entries);
    return true;
}

} // namespace asset_pipe

