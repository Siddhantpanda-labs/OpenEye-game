#pragma once

#include "asset/core/asset_type.h"
#include "asset/core/guid.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace asset {

/** Per-asset entry built from .meta + path (relative to scan root). */
struct AssetInfo {
    Guid guid{};
    AssetType type = AssetType::Unknown;
    std::string path;
    std::vector<Guid> dependencies;
    uint64_t lastModified = 0;
    uint64_t fileSize = 0;
};

/** Fingerprint for incremental scan: path → (timestamp, size, guid). */
struct AssetSnapshotEntry {
    std::string path;
    uint64_t timestamp = 0;
    uint64_t size = 0;
    Guid guid{};
};

/**
 * Central index: GUID <-> path, built by scanning Assets/ and reading .meta files.
 * Persists to Library/asset_database.json (or .bin) for fast startup.
 */
class AssetDatabase {
public:
    /** Full scan: read every .meta and build database. Use for cold start. */
    void Scan(const std::string& assetsRoot);

    /**
     * Incremental scan using snapshot: only read .meta when file (timestamp/size) changed.
     * Recommended startup: LoadCache(); if (LoadSnapshot(path)) ScanIncremental(root); else Scan(root);
     * then SaveCache(); SaveSnapshot(path);
     */
    void ScanIncremental(const std::string& assetsRoot);

    /** Load fingerprint table from file (e.g. Library/asset_snapshot.cache). Returns false if file missing or empty. */
    bool LoadSnapshot(const std::string& filePath);

    /** Save fingerprint table for next incremental scan. */
    bool SaveSnapshot(const std::string& filePath) const;

    /** True if this GUID is in the database. */
    bool AssetExists(const Guid& guid) const;

    /** Resolve GUID to asset path (relative to the root used in Scan). Empty if not found. */
    std::string GetPath(const Guid& guid) const;

    /** Resolve path (relative or normalized) to GUID. Returns Invalid() if not found. */
    Guid GetGuid(const std::string& path) const;

    /** Get full info for an asset by GUID. Returns nullptr if not found. */
    const AssetInfo* GetAssetInfo(const Guid& guid) const;

    /** Get direct dependencies of an asset. */
    std::vector<Guid> GetDependencies(const Guid& guid) const;

    /** Get GUIDs that depend on this asset (reverse dependencies). */
    std::vector<Guid> GetDependents(const Guid& guid) const;

    /** All known GUIDs. */
    std::vector<Guid> GetAllGuids() const;

    /** Register a single asset at runtime (e.g. when no .meta exists). Path should be relative to asset root. Returns the GUID. */
    Guid AddRuntimeAsset(const std::string& relativePath, const std::string& assetType);
    /** Register with a specific GUID. Returns false if path or guid already registered. */
    bool AddRuntimeAsset(const std::string& relativePath, const std::string& assetType, const Guid& guid);

    /** Re-check file timestamps; update or remove stale entries. Rebuilds dependents. */
    void ValidateCache(const std::string& assetsRoot);

    /** Save current index to a file (e.g. Library/asset_database.json). Uses simple line-based format. */
    bool SaveCache(const std::string& filePath) const;

    /** Load index from file; replace = true clears first. */
    bool LoadCache(const std::string& filePath, bool replace = true);

    void Clear();

private:
    void BuildDependents();
    void ProcessOneAsset(const std::string& assetsRoot, const std::filesystem::path& p, const std::string& rel,
                        uint64_t timestamp, uint64_t fileSize);

    std::unordered_map<Guid, AssetInfo, Guid::Hash> m_guidToInfo;
    std::unordered_map<std::string, Guid> m_pathToGuid;
    std::unordered_map<Guid, std::vector<Guid>, Guid::Hash> m_dependents;
    /** path (relative) → fingerprint for incremental scan. */
    std::unordered_map<std::string, AssetSnapshotEntry> m_snapshot;

    static std::string NormalizePath(const std::filesystem::path& p);
    static bool IsAssetExtension(const std::filesystem::path& p);
};

} // namespace asset
