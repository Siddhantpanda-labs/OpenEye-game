#pragma once

#include "api/world/All.h"

#include <cstdint>

namespace openeye::editor {

/**
 * Loads and caches light gizmo icons from editor/ui/icon for drawing in the scene view.
 * Textures are suitable for ImGui/ImDrawList (ImTextureID = GLuint when using OpenGL).
 */
class LightGizmoIcons {
public:
    /** Ensure icons are loaded; call once per frame or when drawing. Returns true if all loaded. */
    bool EnsureLoaded();
    /** Get ImTextureID for the given light type. Returns 0 if not loaded. */
    uint64_t GetTexture(::eyelib::world::components::Light::Type type) const;
    /** Release GL textures; call on shutdown if needed. */
    void Shutdown();

private:
    bool TryLoadFromDir(const char* dir);
    bool LoadPngToGlTexture(const char* path, uint32_t* outTexId);

    uint32_t m_directionalTexId = 0;
    uint32_t m_pointTexId = 0;
    uint32_t m_spotTexId = 0;
    bool m_attempted = false;
};

} // namespace openeye::editor
