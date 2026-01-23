#include "FluidSimulation.h"

#include "eyelib/core/Log.h"

#include <algorithm>
#include <cmath>

namespace eyelib {
namespace fluids {

FluidSimulation::FluidSimulation() = default;

FluidSimulation::~FluidSimulation() {
    Shutdown();
}

bool FluidSimulation::Initialize(const FluidGridDesc& desc, FluidType type) {
    if (desc.resolution.x <= 0 || desc.resolution.y <= 0 || desc.resolution.z <= 0) return false;
    m_desc = desc;
    m_type = type;

    const int cellCount = desc.resolution.x * desc.resolution.y * desc.resolution.z;
    m_density.assign(static_cast<size_t>(cellCount), 0.0f);
    m_velocity.assign(static_cast<size_t>(cellCount), math::Vec3(0.0f, 0.0f, 0.0f));
    m_initialized = true;
    return true;
}

void FluidSimulation::Shutdown() {
    m_density.clear();
    m_velocity.clear();
    m_initialized = false;
}

bool FluidSimulation::IsInitialized() const {
    return m_initialized;
}

void FluidSimulation::Step(float dt) {
    if (!m_initialized) return;
    if (dt <= 0.0f) return;

    // Very small placeholder: simple dissipation.
    const float densityDissipation = 0.995f;
    for (auto& d : m_density) d *= std::pow(densityDissipation, dt * 60.0f);

    const float velDamp = 0.99f;
    for (auto& v : m_velocity) v *= std::pow(velDamp, dt * 60.0f);
}

void FluidSimulation::InjectDensity(const math::Vec3& pos01, float radius01, float amount) {
    if (!m_initialized) return;
    if (radius01 <= 0.0f) return;

    const math::IVec3 res = m_desc.resolution;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    const float r = glm::clamp(radius01, 0.0f, 1.0f);

    const math::Vec3 centerF(
        p.x * (res.x - 1),
        p.y * (res.y - 1),
        p.z * (res.z - 1)
    );
    const float radiusCells = r * static_cast<float>(std::max({ res.x, res.y, res.z }));

    const int minX = std::max(0, static_cast<int>(std::floor(centerF.x - radiusCells)));
    const int maxX = std::min(res.x - 1, static_cast<int>(std::ceil(centerF.x + radiusCells)));
    const int minY = std::max(0, static_cast<int>(std::floor(centerF.y - radiusCells)));
    const int maxY = std::min(res.y - 1, static_cast<int>(std::ceil(centerF.y + radiusCells)));
    const int minZ = std::max(0, static_cast<int>(std::floor(centerF.z - radiusCells)));
    const int maxZ = std::min(res.z - 1, static_cast<int>(std::ceil(centerF.z + radiusCells)));

    const float r2 = radiusCells * radiusCells;
    for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const math::Vec3 c(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                const math::Vec3 d = c - centerF;
                const float dist2 = glm::dot(d, d);
                if (dist2 > r2) continue;
                const float falloff = 1.0f - std::sqrt(dist2) / std::max(0.0001f, radiusCells);
                m_density[static_cast<size_t>(Index(x, y, z))] += amount * falloff;
            }
        }
    }
}

void FluidSimulation::InjectVelocity(const math::Vec3& pos01, float radius01, const math::Vec3& velocity) {
    if (!m_initialized) return;
    if (radius01 <= 0.0f) return;

    const math::IVec3 res = m_desc.resolution;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    const float r = glm::clamp(radius01, 0.0f, 1.0f);

    const math::Vec3 centerF(
        p.x * (res.x - 1),
        p.y * (res.y - 1),
        p.z * (res.z - 1)
    );
    const float radiusCells = r * static_cast<float>(std::max({ res.x, res.y, res.z }));

    const int minX = std::max(0, static_cast<int>(std::floor(centerF.x - radiusCells)));
    const int maxX = std::min(res.x - 1, static_cast<int>(std::ceil(centerF.x + radiusCells)));
    const int minY = std::max(0, static_cast<int>(std::floor(centerF.y - radiusCells)));
    const int maxY = std::min(res.y - 1, static_cast<int>(std::ceil(centerF.y + radiusCells)));
    const int minZ = std::max(0, static_cast<int>(std::floor(centerF.z - radiusCells)));
    const int maxZ = std::min(res.z - 1, static_cast<int>(std::ceil(centerF.z + radiusCells)));

    const float r2 = radiusCells * radiusCells;
    for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                const math::Vec3 c(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                const math::Vec3 d = c - centerF;
                const float dist2 = glm::dot(d, d);
                if (dist2 > r2) continue;
                const float falloff = 1.0f - std::sqrt(dist2) / std::max(0.0001f, radiusCells);
                m_velocity[static_cast<size_t>(Index(x, y, z))] += velocity * falloff;
            }
        }
    }
}

void FluidSimulation::ClearDensity() {
    if (!m_initialized) return;
    std::fill(m_density.begin(), m_density.end(), 0.0f);
}

FluidGridDesc FluidSimulation::GetDesc() const {
    return m_desc;
}

FluidType FluidSimulation::GetType() const {
    return m_type;
}

float FluidSimulation::SampleDensity(const math::Vec3& pos01) const {
    if (!m_initialized) return 0.0f;
    const math::IVec3 res = m_desc.resolution;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    const int x = static_cast<int>(std::round(p.x * (res.x - 1)));
    const int y = static_cast<int>(std::round(p.y * (res.y - 1)));
    const int z = static_cast<int>(std::round(p.z * (res.z - 1)));
    return m_density[static_cast<size_t>(Index(x, y, z))];
}

int FluidSimulation::Index(int x, int y, int z) const {
    const math::IVec3 r = m_desc.resolution;
    x = std::clamp(x, 0, r.x - 1);
    y = std::clamp(y, 0, r.y - 1);
    z = std::clamp(z, 0, r.z - 1);
    return x + y * r.x + z * r.x * r.y;
}

math::IVec3 FluidSimulation::ClampCell(const math::IVec3& c) const {
    const math::IVec3 r = m_desc.resolution;
    return math::IVec3(
        std::clamp(c.x, 0, r.x - 1),
        std::clamp(c.y, 0, r.y - 1),
        std::clamp(c.z, 0, r.z - 1)
    );
}

} // namespace fluids
} // namespace eyelib

