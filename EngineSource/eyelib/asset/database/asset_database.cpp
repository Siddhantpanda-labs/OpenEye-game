#include "asset/database/asset_database.h"
#include "asset/meta/meta_file.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace asset {

namespace fs = std::filesystem;

namespace {

const char* const kAssetExtensions[] = {
    ".omesh", ".mat", ".png", ".jpg", ".jpeg", ".anim", ".oea"
};

} // namespace

std::string AssetDatabase::NormalizePath(const fs::path& p) {
    std::string s = p.generic_string();
    if (s.size() > 1 && s[0] == '.' && (s[1] == '/' || s[1] == '\\'))
        s = s.substr(2);
    return s;
}

bool AssetDatabase::IsAssetExtension(const fs::path& p) {
    std::string ext = p.extension().generic_string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const char* e : kAssetExtensions) {
        if (ext == e) return true;
    }
    return false;
}

void AssetDatabase::BuildDependents() {
    m_dependents.clear();
    for (const auto& kv : m_guidToInfo) {
        const Guid& asset = kv.first;
        for (const Guid& dep : kv.second.dependencies)
            m_dependents[dep].push_back(asset);
    }
}

void AssetDatabase::Scan(const std::string& assetsRoot) {
    m_guidToInfo.clear();
    m_pathToGuid.clear();
    m_dependents.clear();

    fs::path root(assetsRoot);
    if (!fs::exists(root) || !fs::is_directory(root)) return;

    for (fs::recursive_directory_iterator it(root); it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        fs::path p = it->path();
        if (!IsAssetExtension(p)) continue;

        fs::path metaPath = p;
        metaPath += ".meta";
        if (!fs::exists(metaPath)) continue;

        MetaFile meta;
        if (!ReadMetaFile(metaPath.generic_string(), meta)) continue;

        Guid g = Guid::FromString(meta.guid);
        if (!g.IsValid()) continue;

        std::string rel = fs::relative(p, root).generic_string();
        if (rel.empty() || fs::path(rel).is_absolute()) continue;
        if (m_guidToInfo.count(g)) {
            std::cerr << "[AssetDatabase] Duplicate GUID detected: " << g.ToString() << " path: " << rel << std::endl;
            continue;
        }

        AssetInfo info;
        info.guid = g;
        info.type = AssetTypeFromString(meta.type);
        info.path = rel;
        for (const std::string& d : meta.dependencies) {
            Guid dg = Guid::FromString(d);
            if (dg.IsValid()) info.dependencies.push_back(dg);
        }
        try {
            info.lastModified = static_cast<uint64_t>(fs::last_write_time(p).time_since_epoch().count());
            info.fileSize = static_cast<uint64_t>(fs::file_size(p));
        } catch (...) {}

        m_guidToInfo[g] = info;
        m_pathToGuid[rel] = g;
        m_snapshot[rel] = AssetSnapshotEntry{ rel, info.lastModified, info.fileSize, g };
    }
    BuildDependents();
}

bool AssetDatabase::AssetExists(const Guid& guid) const {
    return m_guidToInfo.count(guid) != 0;
}

std::string AssetDatabase::GetPath(const Guid& guid) const {
    auto it = m_guidToInfo.find(guid);
    if (it == m_guidToInfo.end()) return {};
    return it->second.path;
}

Guid AssetDatabase::GetGuid(const std::string& path) const {
    std::string norm = path;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    if (norm.size() > 1 && norm[0] == '.' && norm[1] == '/') norm = norm.substr(2);
    auto it = m_pathToGuid.find(norm);
    if (it != m_pathToGuid.end()) return it->second;
    return Guid::Invalid();
}

const AssetInfo* AssetDatabase::GetAssetInfo(const Guid& guid) const {
    auto it = m_guidToInfo.find(guid);
    if (it == m_guidToInfo.end()) return nullptr;
    return &it->second;
}

std::vector<Guid> AssetDatabase::GetDependencies(const Guid& guid) const {
    auto it = m_guidToInfo.find(guid);
    if (it == m_guidToInfo.end()) return {};
    return it->second.dependencies;
}

std::vector<Guid> AssetDatabase::GetDependents(const Guid& guid) const {
    auto it = m_dependents.find(guid);
    if (it == m_dependents.end()) return {};
    return it->second;
}

std::vector<Guid> AssetDatabase::GetAllGuids() const {
    std::vector<Guid> out;
    out.reserve(m_guidToInfo.size());
    for (const auto& kv : m_guidToInfo) out.push_back(kv.first);
    return out;
}

Guid AssetDatabase::AddRuntimeAsset(const std::string& relativePath, const std::string& assetType) {
    std::string norm = relativePath;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    while (norm.size() > 1 && norm[0] == '/') norm.erase(0, 1);
    if (norm.empty()) return Guid::Invalid();
    auto it = m_pathToGuid.find(norm);
    if (it != m_pathToGuid.end()) return it->second;
    Guid g = Guid::Generate();
    AssetInfo info;
    info.guid = g;
    info.type = AssetTypeFromString(assetType);
    info.path = norm;
    m_guidToInfo[g] = info;
    m_pathToGuid[norm] = g;
    BuildDependents();
    return g;
}

bool AssetDatabase::AddRuntimeAsset(const std::string& relativePath, const std::string& assetType, const Guid& guid) {
    if (!guid.IsValid()) return false;
    std::string norm = relativePath;
    std::replace(norm.begin(), norm.end(), '\\', '/');
    while (norm.size() > 1 && norm[0] == '/') norm.erase(0, 1);
    if (norm.empty()) return false;
    if (m_pathToGuid.count(norm) || m_guidToInfo.count(guid)) return false;
    AssetInfo info;
    info.guid = guid;
    info.type = AssetTypeFromString(assetType);
    info.path = norm;
    m_guidToInfo[guid] = info;
    m_pathToGuid[norm] = guid;
    BuildDependents();
    return true;
}

bool AssetDatabase::SaveCache(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f) return false;
    for (const auto& kv : m_guidToInfo) {
        const AssetInfo& info = kv.second;
        f << "G\t" << info.guid.ToString() << "\n";
        f << "P\t" << info.path << "\n";
        f << "T\t" << ToString(info.type) << "\n";
        f << "M\t" << info.lastModified << "\n";
        f << "F\t" << info.fileSize << "\n";
        f << "D";
        for (const Guid& d : info.dependencies) f << "\t" << d.ToString();
        f << "\nE\n";
    }
    return f.good();
}

bool AssetDatabase::LoadCache(const std::string& filePath, bool replace) {
    if (replace) Clear();
    std::ifstream f(filePath);
    if (!f) return false;
    std::string line;
    AssetInfo info;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        if (line.size() >= 2 && line[1] == '\t') {
            std::string value = line.substr(2);
            switch (line[0]) {
                case 'G': info = AssetInfo{}; info.guid = Guid::FromString(value); break;
                case 'P': info.path = value; break;
                case 'T': info.type = AssetTypeFromString(value); break;
                case 'M': info.lastModified = static_cast<uint64_t>(std::stoull(value)); break;
                case 'F': info.fileSize = static_cast<uint64_t>(std::stoull(value)); break;
                case 'D': {
                    info.dependencies.clear();
                    std::istringstream iss(line.substr(2));
                    std::string depStr;
                    while (iss >> depStr) {
                        Guid dg = Guid::FromString(depStr);
                        if (dg.IsValid()) info.dependencies.push_back(dg);
                    }
                    break;
                }
                case 'E':
                    if (info.guid.IsValid() && !info.path.empty()) {
                        m_guidToInfo[info.guid] = info;
                        m_pathToGuid[info.path] = info.guid;
                    }
                    info = AssetInfo{};
                    break;
                default: break;
            }
        }
    }
    BuildDependents();
    return true;
}

void AssetDatabase::Clear() {
    m_guidToInfo.clear();
    m_pathToGuid.clear();
    m_dependents.clear();
    m_snapshot.clear();
}

bool AssetDatabase::LoadSnapshot(const std::string& filePath) {
    m_snapshot.clear();
    std::ifstream f(filePath);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        size_t p1 = line.find('\t');
        if (p1 == std::string::npos) continue;
        size_t p2 = line.find('\t', p1 + 1);
        if (p2 == std::string::npos) continue;
        size_t p3 = line.find('\t', p2 + 1);
        if (p3 == std::string::npos) continue;
        AssetSnapshotEntry e;
        e.path = line.substr(0, p1);
        e.timestamp = static_cast<uint64_t>(std::stoull(line.substr(p1 + 1, p2 - p1 - 1)));
        e.size = static_cast<uint64_t>(std::stoull(line.substr(p2 + 1, p3 - p2 - 1)));
        e.guid = Guid::FromString(line.substr(p3 + 1));
        if (e.guid.IsValid() && !e.path.empty())
            m_snapshot[e.path] = e;
    }
    return true;
}

bool AssetDatabase::SaveSnapshot(const std::string& filePath) const {
    std::ofstream f(filePath);
    if (!f) return false;
    for (const auto& kv : m_guidToInfo) {
        const AssetInfo& info = kv.second;
        f << info.path << "\t" << info.lastModified << "\t" << info.fileSize << "\t" << info.guid.ToString() << "\n";
    }
    return f.good();
}

void AssetDatabase::ProcessOneAsset(const std::string& assetsRoot, const fs::path& p, const std::string& rel,
                                   uint64_t timestamp, uint64_t fileSize) {
    fs::path metaPath = p;
    metaPath += ".meta";
    if (!fs::exists(metaPath)) return;
    MetaFile meta;
    if (!ReadMetaFile(metaPath.generic_string(), meta)) return;

    Guid g = Guid::FromString(meta.guid);
    if (!g.IsValid()) return;

    auto it = m_guidToInfo.find(g);
    if (it != m_guidToInfo.end() && it->second.path != rel) {
        std::cerr << "[AssetDatabase] Duplicate GUID detected: " << g.ToString() << " path: " << rel << std::endl;
        return;
    }

    AssetInfo info;
    info.guid = g;
    info.type = AssetTypeFromString(meta.type);
    info.path = rel;
    info.lastModified = timestamp;
    info.fileSize = fileSize;
    for (const std::string& d : meta.dependencies) {
        Guid dg = Guid::FromString(d);
        if (dg.IsValid()) info.dependencies.push_back(dg);
    }

    m_guidToInfo[g] = info;
    m_pathToGuid[rel] = g;
    m_snapshot[rel] = AssetSnapshotEntry{ rel, timestamp, fileSize, g };
}

void AssetDatabase::ScanIncremental(const std::string& assetsRoot) {
    fs::path root(assetsRoot);
    if (!fs::exists(root) || !fs::is_directory(root)) return;

    for (fs::recursive_directory_iterator it(root); it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        fs::path p = it->path();
        if (!IsAssetExtension(p)) continue;

        std::string rel = fs::relative(p, root).generic_string();
        if (rel.empty() || fs::path(rel).is_absolute()) continue;

        uint64_t timestamp = 0, fileSize = 0;
        try {
            timestamp = static_cast<uint64_t>(fs::last_write_time(p).time_since_epoch().count());
            fileSize = static_cast<uint64_t>(fs::file_size(p));
        } catch (...) { continue; }

        auto snapIt = m_snapshot.find(rel);
        if (snapIt != m_snapshot.end() && snapIt->second.timestamp == timestamp && snapIt->second.size == fileSize)
            continue;

        ProcessOneAsset(assetsRoot, p, rel, timestamp, fileSize);
    }

    std::vector<std::string> toRemove;
    for (const auto& kv : m_snapshot) {
        fs::path fullPath = root / kv.first;
        if (!fs::exists(fullPath))
            toRemove.push_back(kv.first);
    }
    for (const std::string& path : toRemove) {
        auto it = m_snapshot.find(path);
        if (it != m_snapshot.end()) {
            Guid g = it->second.guid;
            m_snapshot.erase(it);
            auto dbIt = m_guidToInfo.find(g);
            if (dbIt != m_guidToInfo.end()) {
                m_pathToGuid.erase(dbIt->second.path);
                m_guidToInfo.erase(dbIt);
            }
        }
    }

    BuildDependents();
}

void AssetDatabase::ValidateCache(const std::string& assetsRoot) {
    fs::path root(assetsRoot);
    std::vector<Guid> toRemove;
    for (auto& kv : m_guidToInfo) {
        const Guid& guid = kv.first;
        AssetInfo& info = kv.second;
        fs::path fullPath = root / info.path;
        if (!fs::exists(fullPath)) {
            toRemove.push_back(guid);
            continue;
        }
        uint64_t currentMod = 0;
        try {
            currentMod = static_cast<uint64_t>(fs::last_write_time(fullPath).time_since_epoch().count());
        } catch (...) { continue; }
        if (currentMod == info.lastModified) continue;
        fs::path metaPath = fullPath;
        metaPath += ".meta";
        if (!fs::exists(metaPath)) {
            toRemove.push_back(guid);
            continue;
        }
        MetaFile meta;
        if (!ReadMetaFile(metaPath.generic_string(), meta)) {
            toRemove.push_back(guid);
            continue;
        }
        info.type = AssetTypeFromString(meta.type);
        info.dependencies.clear();
        for (const std::string& d : meta.dependencies) {
            Guid dg = Guid::FromString(d);
            if (dg.IsValid()) info.dependencies.push_back(dg);
        }
        info.lastModified = currentMod;
        try {
            info.fileSize = static_cast<uint64_t>(fs::file_size(fullPath));
        } catch (...) {}
    }
    for (const Guid& g : toRemove) {
        auto it = m_guidToInfo.find(g);
        if (it != m_guidToInfo.end()) {
            m_pathToGuid.erase(it->second.path);
            m_guidToInfo.erase(it);
        }
    }
    BuildDependents();
}

} // namespace asset
