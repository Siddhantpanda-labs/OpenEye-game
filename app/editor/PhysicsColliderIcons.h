#pragma once

#include <cstdint>

namespace openeye::editor {

/** Collider shape for icon lookup. */
enum class ColliderIconShape { Box, Sphere, Capsule, Mesh };

/**
 * Loads and caches physics collider icons from editor/icons/physics for Add Component menu.
 */
class PhysicsColliderIcons {
public:
    /** Load icons if not already loaded. Returns true when all requested icons are loaded. */
    bool EnsureLoaded();
    /** Get texture ID for the given shape (for ImGui::Image). Returns 0 if not loaded. */
    uint64_t GetTexture(ColliderIconShape shape) const;
    void Shutdown();

private:
    bool TryLoadFromDir(const char* dir);
    bool LoadPngToGlTexture(const char* path, uint32_t* outTexId);

    uint32_t m_boxTexId = 0;
    uint32_t m_sphereTexId = 0;
    uint32_t m_capsuleTexId = 0;
    uint32_t m_meshTexId = 0;
    bool m_attempted = false;
};

} // namespace openeye::editor
