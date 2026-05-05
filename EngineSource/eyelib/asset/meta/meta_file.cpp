#include "asset/meta/meta_file.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace asset {

namespace {

std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end == std::string::npos ? std::string::npos : end - start + 1);
}

bool ParseKeyValue(const std::string& line, std::string& key, std::string& value) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return false;
    key = Trim(line.substr(0, colon));
    value = Trim(line.substr(colon + 1));
    return !key.empty();
}

} // namespace

bool ReadMetaFile(const std::string& path, MetaFile& out) {
    out = MetaFile{};
    std::ifstream f(path);
    if (!f) return false;

    std::string line;
    bool inDeps = false;

    while (std::getline(f, line)) {
        std::string trimmed = Trim(line);
        if (trimmed.empty()) continue;

        if (inDeps) {
            if (trimmed.size() >= 2 && trimmed[0] == '-' && (trimmed[1] == ' ' || trimmed[1] == '\t')) {
                std::string dep = Trim(trimmed.substr(1));
                if (!dep.empty()) out.dependencies.push_back(dep);
            } else {
                inDeps = false;
            }
        }

        if (!inDeps) {
            std::string key, value;
            if (ParseKeyValue(line, key, value)) {
                if (key == "guid") out.guid = value;
                else if (key == "type") out.type = value;
                else if (key == "importer") out.importer = value;
                else if (key == "version") out.version = std::stoi(value);
                else if (key == "dependencies") inDeps = true;
            }
        }
    }
    return !out.guid.empty();
}

bool WriteMetaFile(const std::string& path, const MetaFile& meta) {
    std::ofstream f(path);
    if (!f) return false;

    f << "guid: " << meta.guid << "\n";
    f << "type: " << meta.type << "\n";
    if (!meta.dependencies.empty()) {
        f << "dependencies:\n";
        for (const auto& d : meta.dependencies)
            f << "  - " << d << "\n";
    }
    if (!meta.importer.empty()) f << "importer: " << meta.importer << "\n";
    f << "version: " << meta.version << "\n";
    return f.good();
}

std::string GenerateGuidString() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    uint64_t high = dis(gen);
    uint64_t low = dis(gen);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8) << ((high >> 32) & 0xFFFFFFFFu) << "-"
        << std::setw(4) << ((high >> 16) & 0xFFFFu) << "-"
        << std::setw(4) << (high & 0xFFFFu) << "-"
        << std::setw(4) << ((low >> 48) & 0xFFFFu) << "-"
        << std::setw(12) << (low & 0xFFFFFFFFFFFFull);
    return oss.str();
}

} // namespace asset
