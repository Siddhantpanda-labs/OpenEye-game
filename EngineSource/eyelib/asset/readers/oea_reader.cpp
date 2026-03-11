#include "asset/readers/oea_reader.h"
#include "asset/core/oea_format.h"

#include <filesystem>
#include <fstream>
#include <vector>
#ifdef ASSET_HAVE_ZSTD
#include <zstd.h>
#endif

namespace asset {

namespace {
namespace fs = std::filesystem;

bool ReadHeaderAndIndex(std::ifstream& in, std::vector<OeaEntry>& outEntries) {
    using namespace OeaFormat;
    OeaHeader header{};
    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
    if (header.magic != kMagic) return false;
    if (header.version != kVersion && header.version != kVersionV1) return false;

    outEntries.clear();
    outEntries.reserve(header.entryCount);
    for (uint32_t i = 0; i < header.entryCount; ++i) {
        OeaEntryRecord rec{};
        if (header.version == kVersionV1) {
            struct OeaEntryRecordV1 {
                uint32_t type;
                uint32_t nameLen;
                uint64_t offset;
                uint64_t size;
            } recV1{};
            if (!in.read(reinterpret_cast<char*>(&recV1), sizeof(recV1))) return false;
            rec.type = recV1.type;
            rec.nameLen = recV1.nameLen;
            rec.flags = 0;
            rec.offset = recV1.offset;
            rec.size = recV1.size;
            rec.uncompressedSize = recV1.size;
        } else {
            if (!in.read(reinterpret_cast<char*>(&rec), sizeof(rec))) return false;
        }
        OeaEntry e;
        e.type = rec.type;
        e.flags = rec.flags;
        e.offset = rec.offset;
        e.size = rec.size;
        e.uncompressedSize = rec.uncompressedSize;
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

bool OeaReader::ReadEntryData(const std::string& packagePath, const std::string& entryName, std::vector<uint8_t>& outData) {
    std::ifstream in(packagePath, std::ios::binary);
    if (!in) return false;
    std::vector<OeaEntry> entries;
    if (!ReadHeaderAndIndex(in, entries)) return false;

    const OeaEntry* selected = nullptr;
    for (const auto& e : entries) {
        if (e.name == entryName) {
            selected = &e;
            break;
        }
    }
    if (!selected) return false;

    in.seekg(static_cast<std::streamoff>(selected->offset), std::ios::beg);
    if (!in) return false;

    std::vector<uint8_t> stored(static_cast<size_t>(selected->size));
    if (!stored.empty() &&
        !in.read(reinterpret_cast<char*>(stored.data()), static_cast<std::streamsize>(stored.size())))
        return false;

    if ((selected->flags & OeaFormat::kFlagCompressedZstd) != 0) {
#ifdef ASSET_HAVE_ZSTD
        std::vector<uint8_t> decoded(static_cast<size_t>(selected->uncompressedSize));
        const size_t dsize = ZSTD_decompress(decoded.data(), decoded.size(), stored.data(), stored.size());
        if (ZSTD_isError(dsize) || dsize != decoded.size()) return false;
        outData = std::move(decoded);
        return true;
#else
        return false;
#endif
    }
    outData = std::move(stored);
    return true;
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

        std::vector<uint8_t> stored(static_cast<size_t>(e.size));
        if (!stored.empty() &&
            !in.read(reinterpret_cast<char*>(stored.data()), static_cast<std::streamsize>(stored.size())))
            return false;

        if ((e.flags & OeaFormat::kFlagCompressedZstd) != 0) {
#ifdef ASSET_HAVE_ZSTD
            std::vector<uint8_t> decoded(static_cast<size_t>(e.uncompressedSize));
            const size_t dsize = ZSTD_decompress(decoded.data(), decoded.size(), stored.data(), stored.size());
            if (ZSTD_isError(dsize) || dsize != decoded.size()) return false;
            if (!decoded.empty() &&
                !out.write(reinterpret_cast<const char*>(decoded.data()), static_cast<std::streamsize>(decoded.size())))
                return false;
#else
            return false;
#endif
        } else {
            if (!stored.empty() &&
                !out.write(reinterpret_cast<const char*>(stored.data()), static_cast<std::streamsize>(stored.size())))
                return false;
        }
    }
    if (outEntries) *outEntries = std::move(entries);
    return true;
}

} // namespace asset

