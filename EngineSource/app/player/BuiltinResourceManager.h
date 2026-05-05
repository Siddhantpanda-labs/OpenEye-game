#pragma once

#include "api/geometry/BuiltinMeshIds.h"
#include "api/render/Material.h"
#include "api/render/Mesh.h"
#include "api/render/RenderDevice.h"
#include "api/render/Shader.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace openeye::player {

/**
 * Single place for builtin primitives (Plane, Cube, Sphere, etc.) and default/skinned materials.
 * Create once, set device, call InitializeDefaultResources(); then use Get* for the lifetime of the player.
 */
class BuiltinResourceManager {
public:
    struct PrimitiveEntry {
        std::shared_ptr<eyelib::api::Mesh> mesh;
        std::shared_ptr<eyelib::api::Material> material;
    };

    BuiltinResourceManager() = default;

    void SetDevice(eyelib::api::RenderDevice device) { m_device = device; }

    /** Create default material and skinned shader once. Call after SetDevice. */
    void InitializeDefaultResources();

    std::shared_ptr<eyelib::api::Material> GetDefaultMaterial();
    std::shared_ptr<eyelib::api::Shader> GetSkinnedShader();
    /** Returns mesh + default material for builtin id (e.g. builtin://Cube), or empty entry if unknown. */
    PrimitiveEntry GetPrimitiveEntry(const std::string& meshId);

private:
    void EnsureDefaultMaterial();
    void EnsureSkinnedMaterial();

    eyelib::api::RenderDevice m_device;
    std::shared_ptr<eyelib::api::Shader> m_defaultShader;
    std::shared_ptr<eyelib::api::Material> m_defaultMaterial;
    std::shared_ptr<eyelib::api::Shader> m_skinnedShader;
    std::unordered_map<std::string, PrimitiveEntry> m_primitiveCache;
};

} // namespace openeye::player
