#include "FluidSimulation.h"

#include "eyelib/core/Log.h"

#include <algorithm>
#include <cmath>

namespace eyelib {
namespace fluids {

namespace {
static float Lerp(float a, float b, float t) { return a + (b - a) * t; }
}

FluidSimulation::FluidSimulation() = default;

FluidSimulation::~FluidSimulation() {
    Shutdown();
}

bool FluidSimulation::Initialize(const FluidGridDesc& desc, FluidType type, const FluidParams& params) {
    if (desc.resolution.x <= 0 || desc.resolution.y <= 0 || desc.resolution.z <= 0) return false;
    m_desc = desc;
    m_type = type;
    m_params = params;

    const int cellCount = desc.resolution.x * desc.resolution.y * desc.resolution.z;
    m_density.assign(static_cast<size_t>(cellCount), 0.0f);
    m_temperature.assign(static_cast<size_t>(cellCount), 0.0f);
    m_velocity.assign(static_cast<size_t>(cellCount), math::Vec3(0.0f));
    m_velocity0.assign(static_cast<size_t>(cellCount), math::Vec3(0.0f));
    m_density0.assign(static_cast<size_t>(cellCount), 0.0f);
    m_temperature0.assign(static_cast<size_t>(cellCount), 0.0f);
    m_pressure.assign(static_cast<size_t>(cellCount), 0.0f);
    m_divergence.assign(static_cast<size_t>(cellCount), 0.0f);
    m_obstacles.assign(static_cast<size_t>(cellCount), 0);
    m_obstacleSdf.clear();
    m_obstacleThreshold = 0.0f;
    m_initialized = true;
    return true;
}

void FluidSimulation::Shutdown() {
    m_density.clear();
    m_temperature.clear();
    m_velocity.clear();
    m_velocity0.clear();
    m_density0.clear();
    m_temperature0.clear();
    m_pressure.clear();
    m_divergence.clear();
    m_obstacles.clear();
    m_obstacleSdf.clear();
    m_initialized = false;
}

bool FluidSimulation::IsInitialized() const {
    return m_initialized;
}

void FluidSimulation::Step(float dt) {
    if (!m_initialized || dt <= 0.0f) return;

    ApplyBuoyancy(dt);
    ApplyVorticity(dt);

    DiffuseVelocity(dt);
    ProjectVelocity();
    AdvectVelocity(dt);
    ProjectVelocity();

    DiffuseScalar(dt, m_density0, m_density, m_params.diffusion);
    std::swap(m_density, m_density0);
    AdvectScalar(dt, m_density0, m_density);
    std::swap(m_density, m_density0);

    DiffuseScalar(dt, m_temperature0, m_temperature, m_params.temperatureDiffusion);
    std::swap(m_temperature, m_temperature0);
    AdvectScalar(dt, m_temperature0, m_temperature);
    std::swap(m_temperature, m_temperature0);

    const float dDiss = std::pow(m_params.densityDissipation, dt * 60.0f);
    const float tDiss = std::pow(m_params.temperatureDissipation, dt * 60.0f);
    for (auto& d : m_density) d *= dDiss;
    for (auto& t : m_temperature) t *= tDiss;

    ApplyObstacles();
}

void FluidSimulation::InjectDensity(const math::Vec3& pos01, float radius01, float amount) {
    if (!m_initialized || radius01 <= 0.0f) return;

    const math::IVec3 res = m_desc.resolution;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    const float r = glm::clamp(radius01, 0.0f, 1.0f);

    const math::Vec3 centerF(p.x * (res.x - 1), p.y * (res.y - 1), p.z * (res.z - 1));
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
                if (IsObstacle(x, y, z)) continue;
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

void FluidSimulation::InjectTemperature(const math::Vec3& pos01, float radius01, float amount) {
    if (!m_initialized || radius01 <= 0.0f) return;

    const math::IVec3 res = m_desc.resolution;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    const float r = glm::clamp(radius01, 0.0f, 1.0f);

    const math::Vec3 centerF(p.x * (res.x - 1), p.y * (res.y - 1), p.z * (res.z - 1));
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
                if (IsObstacle(x, y, z)) continue;
                const math::Vec3 c(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
                const math::Vec3 d = c - centerF;
                const float dist2 = glm::dot(d, d);
                if (dist2 > r2) continue;
                const float falloff = 1.0f - std::sqrt(dist2) / std::max(0.0001f, radiusCells);
                m_temperature[static_cast<size_t>(Index(x, y, z))] += amount * falloff;
            }
        }
    }
}

void FluidSimulation::InjectVelocity(const math::Vec3& pos01, float radius01, const math::Vec3& velocity) {
    if (!m_initialized || radius01 <= 0.0f) return;

    const math::IVec3 res = m_desc.resolution;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    const float r = glm::clamp(radius01, 0.0f, 1.0f);

    const math::Vec3 centerF(p.x * (res.x - 1), p.y * (res.y - 1), p.z * (res.z - 1));
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
                if (IsObstacle(x, y, z)) continue;
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

void FluidSimulation::ClearTemperature() {
    if (!m_initialized) return;
    std::fill(m_temperature.begin(), m_temperature.end(), 0.0f);
}

FluidGridDesc FluidSimulation::GetDesc() const {
    return m_desc;
}

FluidType FluidSimulation::GetType() const {
    return m_type;
}

float FluidSimulation::SampleDensity(const math::Vec3& pos01) const {
    if (!m_initialized) return 0.0f;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    return SampleScalar(m_density, p);
}

float FluidSimulation::SampleTemperature(const math::Vec3& pos01) const {
    if (!m_initialized) return 0.0f;
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    return SampleScalar(m_temperature, p);
}

math::Vec3 FluidSimulation::SampleVelocity(const math::Vec3& pos01) const {
    if (!m_initialized) return math::Vec3(0.0f);
    const math::Vec3 p = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
    return SampleVelocity(m_velocity, p);
}

void FluidSimulation::ClearObstacles() {
    if (!m_initialized) return;
    std::fill(m_obstacles.begin(), m_obstacles.end(), 0);
    m_obstacleSdf.clear();
    m_obstacleThreshold = 0.0f;
}

void FluidSimulation::AddObstacleBox(const FluidObstacleBox& box) {
    if (!m_initialized) return;
    const math::IVec3 res = m_desc.resolution;
    const math::Vec3 center = glm::clamp(box.center01, math::Vec3(0), math::Vec3(1));
    const math::Vec3 size = glm::clamp(box.size01, math::Vec3(0), math::Vec3(1));
    const math::Vec3 minF = (center - size * 0.5f) * math::Vec3(res.x - 1, res.y - 1, res.z - 1);
    const math::Vec3 maxF = (center + size * 0.5f) * math::Vec3(res.x - 1, res.y - 1, res.z - 1);
    const int minX = std::max(0, static_cast<int>(std::floor(minF.x)));
    const int maxX = std::min(res.x - 1, static_cast<int>(std::ceil(maxF.x)));
    const int minY = std::max(0, static_cast<int>(std::floor(minF.y)));
    const int maxY = std::min(res.y - 1, static_cast<int>(std::ceil(maxF.y)));
    const int minZ = std::max(0, static_cast<int>(std::floor(minF.z)));
    const int maxZ = std::min(res.z - 1, static_cast<int>(std::ceil(maxF.z)));
    for (int z = minZ; z <= maxZ; ++z)
        for (int y = minY; y <= maxY; ++y)
            for (int x = minX; x <= maxX; ++x)
                m_obstacles[static_cast<size_t>(Index(x, y, z))] = 1;
}

bool FluidSimulation::SetObstacleSDF(const std::vector<float>& sdf, float threshold) {
    if (!m_initialized) return false;
    const size_t expected = static_cast<size_t>(m_desc.resolution.x * m_desc.resolution.y * m_desc.resolution.z);
    if (sdf.size() != expected) return false;
    m_obstacleSdf = sdf;
    m_obstacleThreshold = threshold;
    for (size_t i = 0; i < expected; ++i) {
        m_obstacles[i] = (m_obstacleSdf[i] < m_obstacleThreshold) ? 1 : 0;
    }
    return true;
}

void FluidSimulation::ClearObstacleSDF() {
    if (!m_initialized) return;
    m_obstacleSdf.clear();
    m_obstacleThreshold = 0.0f;
    std::fill(m_obstacles.begin(), m_obstacles.end(), 0);
}

namespace {
static int WrapIndex(int v, int max) {
    if (max <= 0) return 0;
    int m = v % max;
    return (m < 0) ? m + max : m;
}

static float Wrap01(float v) {
    float w = v - std::floor(v);
    return (w < 0.0f) ? w + 1.0f : w;
}
} // namespace

int FluidSimulation::Index(int x, int y, int z) const {
    const math::IVec3 r = m_desc.resolution;
    if (m_boundaryMode == BoundaryMode::Wrap) {
        x = WrapIndex(x, r.x);
        y = WrapIndex(y, r.y);
        z = WrapIndex(z, r.z);
    } else {
        x = std::clamp(x, 0, r.x - 1);
        y = std::clamp(y, 0, r.y - 1);
        z = std::clamp(z, 0, r.z - 1);
    }
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

bool FluidSimulation::IsObstacle(int x, int y, int z) const {
    const math::IVec3 r = m_desc.resolution;
    if (m_boundaryMode == BoundaryMode::Wrap) {
        return m_obstacles[static_cast<size_t>(Index(x, y, z))] != 0;
    }
    if (x < 0 || y < 0 || z < 0) return true;
    if (x >= r.x || y >= r.y || z >= r.z) return true;
    return m_obstacles[static_cast<size_t>(Index(x, y, z))] != 0;
}

float FluidSimulation::SampleScalar(const std::vector<float>& f, const math::Vec3& pos01) const {
    const math::IVec3 r = m_desc.resolution;
    math::Vec3 p = pos01;
    if (m_boundaryMode == BoundaryMode::Open) {
        if (p.x < 0.0f || p.y < 0.0f || p.z < 0.0f || p.x > 1.0f || p.y > 1.0f || p.z > 1.0f)
            return 0.0f;
    } else if (m_boundaryMode == BoundaryMode::Wrap) {
        p.x = Wrap01(p.x);
        p.y = Wrap01(p.y);
        p.z = Wrap01(p.z);
    } else {
        p = glm::clamp(p, math::Vec3(0.0f), math::Vec3(1.0f));
    }
    const float x = p.x * (r.x - 1);
    const float y = p.y * (r.y - 1);
    const float z = p.z * (r.z - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;
    const float tx = x - x0;
    const float ty = y - y0;
    const float tz = z - z0;

    auto get = [&](int xi, int yi, int zi) {
        if (IsObstacle(xi, yi, zi)) return 0.0f;
        return f[static_cast<size_t>(Index(xi, yi, zi))];
    };

    const float c000 = get(x0, y0, z0);
    const float c100 = get(x1, y0, z0);
    const float c010 = get(x0, y1, z0);
    const float c110 = get(x1, y1, z0);
    const float c001 = get(x0, y0, z1);
    const float c101 = get(x1, y0, z1);
    const float c011 = get(x0, y1, z1);
    const float c111 = get(x1, y1, z1);

    const float c00 = Lerp(c000, c100, tx);
    const float c10 = Lerp(c010, c110, tx);
    const float c01 = Lerp(c001, c101, tx);
    const float c11 = Lerp(c011, c111, tx);
    const float c0 = Lerp(c00, c10, ty);
    const float c1 = Lerp(c01, c11, ty);
    return Lerp(c0, c1, tz);
}

math::Vec3 FluidSimulation::SampleVelocity(const std::vector<math::Vec3>& f, const math::Vec3& pos01) const {
    const math::IVec3 r = m_desc.resolution;
    math::Vec3 p = pos01;
    if (m_boundaryMode == BoundaryMode::Open) {
        if (p.x < 0.0f || p.y < 0.0f || p.z < 0.0f || p.x > 1.0f || p.y > 1.0f || p.z > 1.0f)
            return math::Vec3(0.0f);
    } else if (m_boundaryMode == BoundaryMode::Wrap) {
        p.x = Wrap01(p.x);
        p.y = Wrap01(p.y);
        p.z = Wrap01(p.z);
    } else {
        p = glm::clamp(p, math::Vec3(0.0f), math::Vec3(1.0f));
    }
    const float x = p.x * (r.x - 1);
    const float y = p.y * (r.y - 1);
    const float z = p.z * (r.z - 1);
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;
    const int z1 = z0 + 1;
    const float tx = x - x0;
    const float ty = y - y0;
    const float tz = z - z0;

    auto get = [&](int xi, int yi, int zi) {
        if (IsObstacle(xi, yi, zi)) return math::Vec3(0.0f);
        return f[static_cast<size_t>(Index(xi, yi, zi))];
    };

    const math::Vec3 c000 = get(x0, y0, z0);
    const math::Vec3 c100 = get(x1, y0, z0);
    const math::Vec3 c010 = get(x0, y1, z0);
    const math::Vec3 c110 = get(x1, y1, z0);
    const math::Vec3 c001 = get(x0, y0, z1);
    const math::Vec3 c101 = get(x1, y0, z1);
    const math::Vec3 c011 = get(x0, y1, z1);
    const math::Vec3 c111 = get(x1, y1, z1);

    const math::Vec3 c00 = glm::mix(c000, c100, tx);
    const math::Vec3 c10 = glm::mix(c010, c110, tx);
    const math::Vec3 c01 = glm::mix(c001, c101, tx);
    const math::Vec3 c11 = glm::mix(c011, c111, tx);
    const math::Vec3 c0 = glm::mix(c00, c10, ty);
    const math::Vec3 c1 = glm::mix(c01, c11, ty);
    return glm::mix(c0, c1, tz);
}

void FluidSimulation::AdvectVelocity(float dt) {
    m_velocity0 = m_velocity;
    const math::Vec3 cellSize = m_desc.volumeSize / math::Vec3(
        static_cast<float>(m_desc.resolution.x),
        static_cast<float>(m_desc.resolution.y),
        static_cast<float>(m_desc.resolution.z));
    const math::Vec3 invSize = math::Vec3(1.0f) / m_desc.volumeSize;
    for (int z = 0; z < m_desc.resolution.z; ++z) {
        for (int y = 0; y < m_desc.resolution.y; ++y) {
            for (int x = 0; x < m_desc.resolution.x; ++x) {
                if (IsObstacle(x, y, z)) {
                    m_velocity[Index(x, y, z)] = math::Vec3(0.0f);
                    continue;
                }
                const math::Vec3 pos(
                    (x + 0.5f) * cellSize.x,
                    (y + 0.5f) * cellSize.y,
                    (z + 0.5f) * cellSize.z);
                const math::Vec3 vel = m_velocity0[Index(x, y, z)];
                const math::Vec3 back = pos - vel * dt;
                math::Vec3 pos01 = back * invSize;
                if (m_boundaryMode == BoundaryMode::Solid) {
                    pos01 = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
                } else if (m_boundaryMode == BoundaryMode::Wrap) {
                    pos01.x = Wrap01(pos01.x);
                    pos01.y = Wrap01(pos01.y);
                    pos01.z = Wrap01(pos01.z);
                }
                m_velocity[Index(x, y, z)] = SampleVelocity(m_velocity0, pos01);
            }
        }
    }
}

void FluidSimulation::AdvectScalar(float dt, std::vector<float>& dst, const std::vector<float>& src) {
    const math::Vec3 cellSize = m_desc.volumeSize / math::Vec3(
        static_cast<float>(m_desc.resolution.x),
        static_cast<float>(m_desc.resolution.y),
        static_cast<float>(m_desc.resolution.z));
    const math::Vec3 invSize = math::Vec3(1.0f) / m_desc.volumeSize;
    for (int z = 0; z < m_desc.resolution.z; ++z) {
        for (int y = 0; y < m_desc.resolution.y; ++y) {
            for (int x = 0; x < m_desc.resolution.x; ++x) {
                if (IsObstacle(x, y, z)) {
                    dst[Index(x, y, z)] = 0.0f;
                    continue;
                }
                const math::Vec3 pos(
                    (x + 0.5f) * cellSize.x,
                    (y + 0.5f) * cellSize.y,
                    (z + 0.5f) * cellSize.z);
                const math::Vec3 vel = m_velocity[Index(x, y, z)];
                const math::Vec3 back = pos - vel * dt;
                math::Vec3 pos01 = back * invSize;
                if (m_boundaryMode == BoundaryMode::Solid) {
                    pos01 = glm::clamp(pos01, math::Vec3(0), math::Vec3(1));
                } else if (m_boundaryMode == BoundaryMode::Wrap) {
                    pos01.x = Wrap01(pos01.x);
                    pos01.y = Wrap01(pos01.y);
                    pos01.z = Wrap01(pos01.z);
                }
                dst[Index(x, y, z)] = SampleScalar(src, pos01);
            }
        }
    }
}

void FluidSimulation::DiffuseVelocity(float dt) {
    if (m_params.viscosity <= 0.0f) return;
    m_velocity0 = m_velocity;
    const float a = dt * m_params.viscosity;
    const float c = 1.0f + 6.0f * a;
    for (int iter = 0; iter < m_params.diffusionIterations; ++iter) {
        for (int z = 0; z < m_desc.resolution.z; ++z) {
            for (int y = 0; y < m_desc.resolution.y; ++y) {
                for (int x = 0; x < m_desc.resolution.x; ++x) {
                    if (IsObstacle(x, y, z)) { m_velocity[Index(x, y, z)] = math::Vec3(0.0f); continue; }
                    math::Vec3 sum(0.0f);
                    sum += IsObstacle(x - 1, y, z) ? m_velocity0[Index(x, y, z)] : m_velocity0[Index(x - 1, y, z)];
                    sum += IsObstacle(x + 1, y, z) ? m_velocity0[Index(x, y, z)] : m_velocity0[Index(x + 1, y, z)];
                    sum += IsObstacle(x, y - 1, z) ? m_velocity0[Index(x, y, z)] : m_velocity0[Index(x, y - 1, z)];
                    sum += IsObstacle(x, y + 1, z) ? m_velocity0[Index(x, y, z)] : m_velocity0[Index(x, y + 1, z)];
                    sum += IsObstacle(x, y, z - 1) ? m_velocity0[Index(x, y, z)] : m_velocity0[Index(x, y, z - 1)];
                    sum += IsObstacle(x, y, z + 1) ? m_velocity0[Index(x, y, z)] : m_velocity0[Index(x, y, z + 1)];
                    m_velocity[Index(x, y, z)] = (m_velocity0[Index(x, y, z)] + a * sum) / c;
                }
            }
        }
        m_velocity0.swap(m_velocity);
    }
    m_velocity.swap(m_velocity0);
}

void FluidSimulation::DiffuseScalar(float dt, std::vector<float>& dst, const std::vector<float>& src, float diffusion) {
    if (diffusion <= 0.0f) { dst = src; return; }
    const float a = dt * diffusion;
    const float c = 1.0f + 6.0f * a;
    dst = src;
    std::vector<float> tmp = dst;
    for (int iter = 0; iter < m_params.diffusionIterations; ++iter) {
        for (int z = 0; z < m_desc.resolution.z; ++z) {
            for (int y = 0; y < m_desc.resolution.y; ++y) {
                for (int x = 0; x < m_desc.resolution.x; ++x) {
                    if (IsObstacle(x, y, z)) { tmp[Index(x, y, z)] = 0.0f; continue; }
                    const float center = src[Index(x, y, z)];
                    float sum = 0.0f;
                    sum += IsObstacle(x - 1, y, z) ? center : dst[Index(x - 1, y, z)];
                    sum += IsObstacle(x + 1, y, z) ? center : dst[Index(x + 1, y, z)];
                    sum += IsObstacle(x, y - 1, z) ? center : dst[Index(x, y - 1, z)];
                    sum += IsObstacle(x, y + 1, z) ? center : dst[Index(x, y + 1, z)];
                    sum += IsObstacle(x, y, z - 1) ? center : dst[Index(x, y, z - 1)];
                    sum += IsObstacle(x, y, z + 1) ? center : dst[Index(x, y, z + 1)];
                    tmp[Index(x, y, z)] = (center + a * sum) / c;
                }
            }
        }
        dst.swap(tmp);
    }
}

void FluidSimulation::ProjectVelocity() {
    const math::Vec3 cellSize = m_desc.volumeSize / math::Vec3(
        static_cast<float>(m_desc.resolution.x),
        static_cast<float>(m_desc.resolution.y),
        static_cast<float>(m_desc.resolution.z));
    const float invDx = 1.0f / std::max(0.0001f, cellSize.x);
    const float invDy = 1.0f / std::max(0.0001f, cellSize.y);
    const float invDz = 1.0f / std::max(0.0001f, cellSize.z);

    for (int z = 0; z < m_desc.resolution.z; ++z) {
        for (int y = 0; y < m_desc.resolution.y; ++y) {
            for (int x = 0; x < m_desc.resolution.x; ++x) {
                if (IsObstacle(x, y, z)) { m_divergence[Index(x, y, z)] = 0.0f; continue; }
                const auto vL = IsObstacle(x - 1, y, z) ? math::Vec3(0.0f) : m_velocity[Index(x - 1, y, z)];
                const auto vR = IsObstacle(x + 1, y, z) ? math::Vec3(0.0f) : m_velocity[Index(x + 1, y, z)];
                const auto vD = IsObstacle(x, y - 1, z) ? math::Vec3(0.0f) : m_velocity[Index(x, y - 1, z)];
                const auto vU = IsObstacle(x, y + 1, z) ? math::Vec3(0.0f) : m_velocity[Index(x, y + 1, z)];
                const auto vB = IsObstacle(x, y, z - 1) ? math::Vec3(0.0f) : m_velocity[Index(x, y, z - 1)];
                const auto vF = IsObstacle(x, y, z + 1) ? math::Vec3(0.0f) : m_velocity[Index(x, y, z + 1)];
                const float div = 0.5f * ((vR.x - vL.x) * invDx + (vU.y - vD.y) * invDy + (vF.z - vB.z) * invDz);
                m_divergence[Index(x, y, z)] = -div;
                m_pressure[Index(x, y, z)] = 0.0f;
            }
        }
    }

    for (int iter = 0; iter < m_params.pressureIterations; ++iter) {
        for (int z = 0; z < m_desc.resolution.z; ++z) {
            for (int y = 0; y < m_desc.resolution.y; ++y) {
                for (int x = 0; x < m_desc.resolution.x; ++x) {
                    if (IsObstacle(x, y, z)) { m_pressure[Index(x, y, z)] = 0.0f; continue; }
                    const float pL = IsObstacle(x - 1, y, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x - 1, y, z)];
                    const float pR = IsObstacle(x + 1, y, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x + 1, y, z)];
                    const float pD = IsObstacle(x, y - 1, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y - 1, z)];
                    const float pU = IsObstacle(x, y + 1, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y + 1, z)];
                    const float pB = IsObstacle(x, y, z - 1) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y, z - 1)];
                    const float pF = IsObstacle(x, y, z + 1) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y, z + 1)];
                    m_pressure[Index(x, y, z)] = (m_divergence[Index(x, y, z)] + pL + pR + pD + pU + pB + pF) / 6.0f;
                }
            }
        }
    }

    for (int z = 0; z < m_desc.resolution.z; ++z) {
        for (int y = 0; y < m_desc.resolution.y; ++y) {
            for (int x = 0; x < m_desc.resolution.x; ++x) {
                if (IsObstacle(x, y, z)) { m_velocity[Index(x, y, z)] = math::Vec3(0.0f); continue; }
                const float pL = IsObstacle(x - 1, y, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x - 1, y, z)];
                const float pR = IsObstacle(x + 1, y, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x + 1, y, z)];
                const float pD = IsObstacle(x, y - 1, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y - 1, z)];
                const float pU = IsObstacle(x, y + 1, z) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y + 1, z)];
                const float pB = IsObstacle(x, y, z - 1) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y, z - 1)];
                const float pF = IsObstacle(x, y, z + 1) ? m_pressure[Index(x, y, z)] : m_pressure[Index(x, y, z + 1)];
                math::Vec3 v = m_velocity[Index(x, y, z)];
                v.x -= 0.5f * (pR - pL) * invDx;
                v.y -= 0.5f * (pU - pD) * invDy;
                v.z -= 0.5f * (pF - pB) * invDz;
                m_velocity[Index(x, y, z)] = v;
            }
        }
    }
}

void FluidSimulation::ApplyBuoyancy(float dt) {
    if (m_params.buoyancy == 0.0f && m_params.weight == 0.0f) return;
    for (int z = 0; z < m_desc.resolution.z; ++z) {
        for (int y = 0; y < m_desc.resolution.y; ++y) {
            for (int x = 0; x < m_desc.resolution.x; ++x) {
                if (IsObstacle(x, y, z)) continue;
                const int idx = Index(x, y, z);
                const float temp = m_temperature[idx];
                const float dens = m_density[idx];
                const float buoy = m_params.buoyancy * (temp - m_params.ambientTemperature) - m_params.weight * dens;
                m_velocity[idx].y += buoy * dt;
            }
        }
    }
}

void FluidSimulation::ApplyVorticity(float dt) {
    if (m_params.vorticityStrength <= 0.0f) return;
    const math::IVec3 r = m_desc.resolution;
    std::vector<math::Vec3> curl(static_cast<size_t>(r.x * r.y * r.z), math::Vec3(0.0f));
    std::vector<float> curlMag(curl.size(), 0.0f);

    for (int z = 1; z < r.z - 1; ++z) {
        for (int y = 1; y < r.y - 1; ++y) {
            for (int x = 1; x < r.x - 1; ++x) {
                if (IsObstacle(x, y, z)) continue;
                const math::Vec3 vL = m_velocity[Index(x - 1, y, z)];
                const math::Vec3 vR = m_velocity[Index(x + 1, y, z)];
                const math::Vec3 vD = m_velocity[Index(x, y - 1, z)];
                const math::Vec3 vU = m_velocity[Index(x, y + 1, z)];
                const math::Vec3 vB = m_velocity[Index(x, y, z - 1)];
                const math::Vec3 vF = m_velocity[Index(x, y, z + 1)];
                const math::Vec3 c(
                    0.5f * (vU.z - vD.z) - 0.5f * (vF.y - vB.y),
                    0.5f * (vF.x - vB.x) - 0.5f * (vR.z - vL.z),
                    0.5f * (vR.y - vL.y) - 0.5f * (vU.x - vD.x));
                const int idx = Index(x, y, z);
                curl[idx] = c;
                curlMag[idx] = glm::length(c);
            }
        }
    }

    for (int z = 1; z < r.z - 1; ++z) {
        for (int y = 1; y < r.y - 1; ++y) {
            for (int x = 1; x < r.x - 1; ++x) {
                if (IsObstacle(x, y, z)) continue;
                const float L = curlMag[Index(x - 1, y, z)];
                const float R = curlMag[Index(x + 1, y, z)];
                const float D = curlMag[Index(x, y - 1, z)];
                const float U = curlMag[Index(x, y + 1, z)];
                const float B = curlMag[Index(x, y, z - 1)];
                const float F = curlMag[Index(x, y, z + 1)];
                math::Vec3 grad(0.5f * (R - L), 0.5f * (U - D), 0.5f * (F - B));
                const float len = glm::length(grad);
                if (len > 0.0001f) grad /= len;
                const math::Vec3 force = glm::cross(grad, curl[Index(x, y, z)]) * m_params.vorticityStrength;
                m_velocity[Index(x, y, z)] += force * dt;
            }
        }
    }
}

void FluidSimulation::ApplyObstacles() {
    if (m_obstacles.empty()) return;
    for (int z = 0; z < m_desc.resolution.z; ++z) {
        for (int y = 0; y < m_desc.resolution.y; ++y) {
            for (int x = 0; x < m_desc.resolution.x; ++x) {
                if (!IsObstacle(x, y, z)) continue;
                const int idx = Index(x, y, z);
                m_velocity[idx] = math::Vec3(0.0f);
                m_density[idx] = 0.0f;
                m_temperature[idx] = 0.0f;
            }
        }
    }
}

} // namespace fluids
} // namespace eyelib

