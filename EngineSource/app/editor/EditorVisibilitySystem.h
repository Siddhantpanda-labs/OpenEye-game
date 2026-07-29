#pragma once

#include "api/rendering/RenderScene.h"
#include "api/world/All.h"

#include <cstdint>
#include <unordered_set>
#include <vector>

namespace openeye::editor {

class EditorVisibilitySystem {
public:
    using EntityId = ::eyelib::world::EntityId;

    void BeginFrame(const eyelib::api::world::World::RenderSnapshot& snapshot);
    void BuildCameraVisibility(const eyelib::api::rendering::CameraView& camera);
    bool IsVisible(EntityId entity) const;

    struct Plane {
        float a = 0.f, b = 0.f, c = 0.f, d = 0.f;
    };

private:
    struct Candidate {
        EntityId entity = ::eyelib::world::InvalidEntity;
        eyelib::math::Vec3 position{};
        float radius = 1.0f;
    };

    static void BuildFrustumPlanes(const eyelib::math::Mat4& viewProjection, Plane out[6]);
    static bool SphereIntersectsFrustum(const Plane planes[6], const eyelib::math::Vec3& center, float radius);

    std::vector<Candidate> m_candidates;
    std::vector<uint8_t> m_visibilityMask;
    
    std::vector<eyelib::math::Vec3> m_transformPositions;
    std::vector<uint32_t> m_transformGenerations;
};

} // namespace openeye::editor
