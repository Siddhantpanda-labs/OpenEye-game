#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace eyelib {
namespace render {

/**
 * Watches a source assets/shaders directory and copies changed shader files
 * to a runtime assets/shaders directory so edits in the root repo are
 * reflected where the running app loads assets from.
 *
 * Uses polling: call Tick() periodically (e.g. every frame or every 1–2 s).
 * Only copies .vert, .frag, .glsl, .vsh, .psh (and .comp, .geom, .tesc, .tese) when source is newer than runtime.
 */
class ShaderWatcher {
public:
    ShaderWatcher() = default;
    ~ShaderWatcher() = default;

    /** Source assets directory (e.g. project root "assets" where you edit). Must contain "shaders" subdir. */
    void SetSourceAssetsDir(const std::string& path) { m_sourceAssetsDir = path; }
    /** Runtime assets directory (e.g. cwd "assets" where the app loads from). Will write into "shaders" subdir. */
    void SetRuntimeAssetsDir(const std::string& path) { m_runtimeAssetsDir = path; }

    std::string GetSourceAssetsDir() const { return m_sourceAssetsDir; }
    std::string GetRuntimeAssetsDir() const { return m_runtimeAssetsDir; }

    /** Poll for changes and copy updated shaders. Call every frame or every N ms. Returns number of files copied this tick. */
    int Tick();

    /** Optional: called after a file is copied (path is relative, e.g. "shaders/passes/forward/forward.vert"). */
    void SetOnShaderCopied(std::function<void(const std::string&)> fn) { m_onCopied = std::move(fn); }

private:
    std::string m_sourceAssetsDir;
    std::string m_runtimeAssetsDir;
    std::function<void(const std::string&)> m_onCopied;
    int64_t m_lastPollTime = 0;
    static constexpr int64_t kPollIntervalMs = 800;
};

} // namespace render
} // namespace eyelib
