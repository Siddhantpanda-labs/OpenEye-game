#include "eyelib/render/TiledLightGrid.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace eyelib {
namespace render {

TiledLightGrid::TiledLightGrid() = default;

TiledLightGrid::~TiledLightGrid() {
    // SSBOs cleaned up by caller
}

TiledLightGrid::TiledLightGrid(TiledLightGrid&& other) noexcept
    : m_screenWidth(other.m_screenWidth), m_screenHeight(other.m_screenHeight),
      m_tileCountX(other.m_tileCountX), m_tileCountY(other.m_tileCountY),
      m_tileLightIndices(std::move(other.m_tileLightIndices)),
      m_lightGridFlat(std::move(other.m_lightGridFlat)) {
    other.m_screenWidth = other.m_screenHeight = 0;
    other.m_tileCountX = other.m_tileCountY = 0;
}

TiledLightGrid& TiledLightGrid::operator=(TiledLightGrid&& other) noexcept {
    if (this != &other) {
        m_screenWidth = other.m_screenWidth;
        m_screenHeight = other.m_screenHeight;
        m_tileCountX = other.m_tileCountX;
        m_tileCountY = other.m_tileCountY;
        m_tileLightIndices = std::move(other.m_tileLightIndices);
        m_lightGridFlat = std::move(other.m_lightGridFlat);
        other.m_screenWidth = other.m_screenHeight = 0;
        other.m_tileCountX = other.m_tileCountY = 0;
    }
    return *this;
}

void TiledLightGrid::Initialize(uint32_t screenWidth, uint32_t screenHeight) {
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_tileCountX = (screenWidth + TILE_SIZE - 1) / TILE_SIZE;
    m_tileCountY = (screenHeight + TILE_SIZE - 1) / TILE_SIZE;
    m_tileLightIndices.clear();
    m_tileLightIndices.resize(m_tileCountX * m_tileCountY);
    m_lightGridFlat.clear();
}

void TiledLightGrid::Clear() {
    for (auto& tile : m_tileLightIndices) {
        tile.clear();
    }
    m_lightGridFlat.clear();
}

void TiledLightGrid::CullLightsForTile(uint32_t tileX, uint32_t tileY,
                                       const std::vector<PointLight>& pointLights,
                                       const std::vector<SpotLight>& spotLights,
                                       const math::Mat4& view,
                                       const math::Mat4& projection,
                                       const math::Vec3& cameraPos) {
    (void)view;
    (void)projection;
    (void)cameraPos;
    uint32_t tileIdx = tileY * m_tileCountX + tileX;
    auto& lights = m_tileLightIndices[tileIdx];
    lights.clear();

    float tileMinX = static_cast<float>(tileX * TILE_SIZE) / static_cast<float>(m_screenWidth) * 2.0f - 1.0f;
    float tileMaxX = static_cast<float>((tileX + 1) * TILE_SIZE) / static_cast<float>(m_screenWidth) * 2.0f - 1.0f;
    float tileMinY = 1.0f - static_cast<float>((tileY + 1) * TILE_SIZE) / static_cast<float>(m_screenHeight) * 2.0f;
    float tileMaxY = 1.0f - static_cast<float>(tileY * TILE_SIZE) / static_cast<float>(m_screenHeight) * 2.0f;

    glm::mat4 invViewProj = glm::inverse(glm::mat4(projection) * glm::mat4(view));
    glm::vec4 corners[4] = {
        invViewProj * glm::vec4(tileMinX, tileMinY, -1, 1),
        invViewProj * glm::vec4(tileMaxX, tileMinY, -1, 1),
        invViewProj * glm::vec4(tileMaxX, tileMaxY, -1, 1),
        invViewProj * glm::vec4(tileMinX, tileMaxY, -1, 1)
    };
    for (auto& c : corners) c /= c.w;

    glm::vec3 tileFrustum[8];
    for (int i = 0; i < 4; ++i) {
        glm::vec4 far = invViewProj * glm::vec4(corners[i].x, corners[i].y, 1, 1);
        far /= far.w;
        tileFrustum[i] = glm::vec3(corners[i]);
        tileFrustum[i + 4] = glm::vec3(far);
    }

    uint32_t lightIdx = 0;
    for (const auto& pl : pointLights) {
        if (lights.size() >= MAX_LIGHTS_PER_TILE) break;
        glm::vec3 pos(pl.position.x, pl.position.y, pl.position.z);
        float radius = pl.radius;
        bool intersects = false;
        for (int i = 0; i < 8; ++i) {
            float dist = glm::length(tileFrustum[i] - pos);
            if (dist <= radius) {
                intersects = true;
                break;
            }
        }
        if (!intersects) {
            glm::vec3 tileCenter = (tileFrustum[0] + tileFrustum[2] + tileFrustum[4] + tileFrustum[6]) * 0.25f;
            float distToCenter = glm::length(pos - tileCenter);
            if (distToCenter <= radius * 1.5f) intersects = true;
        }
        if (intersects) lights.push_back(lightIdx);
        ++lightIdx;
    }

    uint32_t spotBase = static_cast<uint32_t>(pointLights.size());
    lightIdx = 0;
    for (const auto& sl : spotLights) {
        if (lights.size() >= MAX_LIGHTS_PER_TILE) break;
        glm::vec3 pos(sl.position.x, sl.position.y, sl.position.z);
        glm::vec3 dir(sl.direction.x, sl.direction.y, sl.direction.z);
        float range = sl.range;
        bool intersects = false;
        for (int i = 0; i < 8; ++i) {
            glm::vec3 toPoint = tileFrustum[i] - pos;
            float dist = glm::length(toPoint);
            if (dist <= range) {
                float cosAngle = glm::dot(glm::normalize(toPoint), dir);
                if (cosAngle >= sl.outerConeCos) {
                    intersects = true;
                    break;
                }
            }
        }
        if (intersects) lights.push_back(spotBase + lightIdx);
        ++lightIdx;
    }
}

void TiledLightGrid::BuildLightGrid(const std::vector<PointLight>& pointLights,
                                    const std::vector<SpotLight>& spotLights,
                                    const math::Mat4& view,
                                    const math::Mat4& projection,
                                    const math::Vec3& cameraPos) {
    Clear();
    for (uint32_t ty = 0; ty < m_tileCountY; ++ty) {
        for (uint32_t tx = 0; tx < m_tileCountX; ++tx) {
            CullLightsForTile(tx, ty, pointLights, spotLights, view, projection, cameraPos);
        }
    }

    m_lightGridFlat.resize(m_tileCountX * m_tileCountY * MAX_LIGHTS_PER_TILE, 0xFF);
    for (uint32_t i = 0; i < m_tileLightIndices.size(); ++i) {
        const auto& tile = m_tileLightIndices[i];
        size_t offset = i * MAX_LIGHTS_PER_TILE;
        for (size_t j = 0; j < tile.size() && j < MAX_LIGHTS_PER_TILE; ++j) {
            m_lightGridFlat[offset + j] = static_cast<uint8_t>(tile[j]);
        }
    }
}

const std::vector<uint32_t>& TiledLightGrid::GetTileLights(uint32_t tileX, uint32_t tileY) const {
    static const std::vector<uint32_t> empty;
    if (tileX >= m_tileCountX || tileY >= m_tileCountY) return empty;
    uint32_t idx = tileY * m_tileCountX + tileX;
    if (idx >= m_tileLightIndices.size()) return empty;
    return m_tileLightIndices[idx];
}

void TiledLightGrid::UploadToGPU(unsigned int& lightIndexSSBO, unsigned int& lightGridSSBO) {
    (void)lightGridSSBO;
    (void)lightIndexSSBO;
    if (m_lightGridFlat.empty()) return;
}

} // namespace render
} // namespace eyelib
