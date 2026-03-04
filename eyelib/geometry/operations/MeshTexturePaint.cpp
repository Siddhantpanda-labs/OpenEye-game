#include "eyelib/geometry/operations/MeshTexturePaint.h"
#include "eyelib/math/AABB.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <cfloat>

namespace eyelib {
namespace geometry {

void MeshTexturePaint::GenerateUVs(assets::EditableMesh& editable, UVProjection projection,
                                   int axis, const Selection& selection) {
    switch (projection) {
        case UVProjection::Planar: {
            math::Vec3 forward, right, up;
            GetAxisVectors(axis, forward, right, up);
            GeneratePlanarUVs(editable, forward, up, selection);
            break;
        }
        case UVProjection::Cylindrical:
            GenerateCylindricalUVs(editable, axis, selection);
            break;
        case UVProjection::Spherical:
            GenerateSphericalUVs(editable, selection);
            break;
        case UVProjection::Box:
            GenerateBoxUVs(editable, selection);
            break;
    }
}

void MeshTexturePaint::GeneratePlanarUVs(assets::EditableMesh& editable, const math::Vec3& normal, const math::Vec3& up,
                                         const Selection& selection) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    math::Vec3 n = glm::normalize(normal);
    math::Vec3 u = glm::normalize(up);
    math::Vec3 r = glm::normalize(glm::cross(n, u));
    u = glm::normalize(glm::cross(r, n)); // Re-orthogonalize
    
    // Determine which vertices to process
    std::unordered_set<uint32_t> verticesToProcess;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToProcess.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToProcess.insert(vid);
            }
        }
    }
    
    // Compute bounds for scaling
    math::AABB bounds = ComputeMeshBounds(editable, selection);
    math::Vec3 size = bounds.GetSize();
    math::Vec3 center = bounds.GetCenter();
    
    // Generate UVs
    for (uint32_t vid : verticesToProcess) {
        if (vid >= vertices.size()) continue;
        
        math::Vec3 pos = vertices[vid].position - center;
        
        // Project onto plane
        float uCoord = glm::dot(pos, r);
        float vCoord = glm::dot(pos, u);
        
        // Normalize to [0, 1] range
        if (size.x > 0.001f) uCoord = (uCoord + size.x * 0.5f) / size.x;
        else uCoord = 0.5f;
        
        if (size.y > 0.001f) vCoord = (vCoord + size.y * 0.5f) / size.y;
        else vCoord = 0.5f;
        
        vertices[vid].uv = math::Vec2(uCoord, vCoord);
    }
    
}

void MeshTexturePaint::GenerateCylindricalUVs(assets::EditableMesh& editable, int axis,
                                              const Selection& selection) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Determine which vertices to process
    std::unordered_set<uint32_t> verticesToProcess;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToProcess.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToProcess.insert(vid);
            }
        }
    }
    
    // Compute bounds
    math::AABB bounds = ComputeMeshBounds(editable, selection);
    math::Vec3 size = bounds.GetSize();
    math::Vec3 center = bounds.GetCenter();
    
    // Generate UVs based on axis
    for (uint32_t vid : verticesToProcess) {
        if (vid >= vertices.size()) continue;
        
        math::Vec3 pos = vertices[vid].position - center;
        float uCoord = 0.0f, vCoord = 0.0f;
        
        if (axis == 0) { // X-axis
            float angle = std::atan2(pos.z, pos.y);
            uCoord = (angle + 3.14159265f) / (2.0f * 3.14159265f);
            vCoord = (pos.x + size.x * 0.5f) / size.x;
        } else if (axis == 1) { // Y-axis
            float angle = std::atan2(pos.x, pos.z);
            uCoord = (angle + 3.14159265f) / (2.0f * 3.14159265f);
            vCoord = (pos.y + size.y * 0.5f) / size.y;
        } else { // Z-axis
            float angle = std::atan2(pos.y, pos.x);
            uCoord = (angle + 3.14159265f) / (2.0f * 3.14159265f);
            vCoord = (pos.z + size.z * 0.5f) / size.z;
        }
        
        vertices[vid].uv = math::Vec2(uCoord, vCoord);
    }
    
}

void MeshTexturePaint::GenerateSphericalUVs(assets::EditableMesh& editable, const Selection& selection) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Determine which vertices to process
    std::unordered_set<uint32_t> verticesToProcess;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToProcess.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToProcess.insert(vid);
            }
        }
    }
    
    // Compute bounds
    math::AABB bounds = ComputeMeshBounds(editable, selection);
    math::Vec3 center = bounds.GetCenter();
    
    // Generate spherical UVs
    for (uint32_t vid : verticesToProcess) {
        if (vid >= vertices.size()) continue;
        
        math::Vec3 pos = vertices[vid].position - center;
        float len = glm::length(pos);
        
        if (len < 0.001f) {
            vertices[vid].uv = math::Vec2(0.5f, 0.5f);
            continue;
        }
        
        math::Vec3 normalized = pos / len;
        
        // Spherical coordinates
        float theta = std::atan2(normalized.x, normalized.z); // Longitude
        float phi = std::acos(normalized.y); // Latitude
        
        // Map to UV space [0, 1]
        float uCoord = (theta + 3.14159265f) / (2.0f * 3.14159265f);
        float vCoord = phi / 3.14159265f;
        
        vertices[vid].uv = math::Vec2(uCoord, vCoord);
    }
    
}

void MeshTexturePaint::GenerateBoxUVs(assets::EditableMesh& editable, const Selection& selection) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Determine which vertices to process
    std::unordered_set<uint32_t> verticesToProcess;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToProcess.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToProcess.insert(vid);
            }
        }
    }
    
    // Compute bounds
    math::AABB bounds = ComputeMeshBounds(editable, selection);
    math::Vec3 size = bounds.GetSize();
    math::Vec3 center = bounds.GetCenter();
    
    // Generate box UVs (6-sided projection)
    for (uint32_t vid : verticesToProcess) {
        if (vid >= vertices.size()) continue;
        
        math::Vec3 pos = vertices[vid].position - center;
        math::Vec3 absPos(std::abs(pos.x), std::abs(pos.y), std::abs(pos.z));
        
        // Determine which face (dominant axis)
        float uCoord = 0.0f, vCoord = 0.0f;
        float offsetU = 0.0f, offsetV = 0.0f;
        
        if (absPos.x >= absPos.y && absPos.x >= absPos.z) {
            // X-face
            if (pos.x > 0) {
                offsetU = 0.0f; // +X face
            } else {
                offsetU = 1.0f / 3.0f; // -X face
            }
            uCoord = (pos.z + size.z * 0.5f) / size.z;
            vCoord = (pos.y + size.y * 0.5f) / size.y;
        } else if (absPos.y >= absPos.x && absPos.y >= absPos.z) {
            // Y-face
            if (pos.y > 0) {
                offsetU = 2.0f / 3.0f; // +Y face
            } else {
                offsetU = 0.5f; // -Y face
            }
            uCoord = (pos.x + size.x * 0.5f) / size.x;
            vCoord = (pos.z + size.z * 0.5f) / size.z;
        } else {
            // Z-face
            if (pos.z > 0) {
                offsetU = 1.0f / 6.0f; // +Z face
            } else {
                offsetU = 5.0f / 6.0f; // -Z face
            }
            uCoord = (pos.x + size.x * 0.5f) / size.x;
            vCoord = (pos.y + size.y * 0.5f) / size.y;
        }
        
        // Scale and offset to fit in [0, 1] with 6 faces
        uCoord = uCoord / 6.0f + offsetU;
        vCoord = vCoord;
        
        vertices[vid].uv = math::Vec2(uCoord, vCoord);
    }
    
}

void MeshTexturePaint::UnwrapUVs(assets::EditableMesh& editable, float padding) {
    // Simplified UV unwrapping: uses planar projection per face
    // Full implementation would use LSCM or ABF algorithms
    
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    const auto& indices = editable.GetIndices();
    
    if (indices.empty() || indices.size() % 3 != 0) return;
    
    // For each face, compute planar UVs
    size_t faceCount = indices.size() / 3;
    for (size_t i = 0; i < faceCount; ++i) {
        uint32_t baseIdx = static_cast<uint32_t>(i * 3);
        uint32_t v0 = indices[baseIdx];
        uint32_t v1 = indices[baseIdx + 1];
        uint32_t v2 = indices[baseIdx + 2];
        
        if (v0 >= vertices.size() || v1 >= vertices.size() || v2 >= vertices.size()) continue;
        
        // Compute face normal
        math::Vec3 p0 = vertices[v0].position;
        math::Vec3 p1 = vertices[v1].position;
        math::Vec3 p2 = vertices[v2].position;
        
        math::Vec3 edge1 = p1 - p0;
        math::Vec3 edge2 = p2 - p0;
        math::Vec3 normal = glm::normalize(glm::cross(edge1, edge2));
        
        // Project face onto plane
        math::Vec3 right = glm::normalize(edge1);
        math::Vec3 up = glm::normalize(glm::cross(normal, right));
        
        // Compute UVs for this face
        float len1 = glm::length(edge1);
        float len2 = glm::length(edge2);
        
        vertices[v0].uv = math::Vec2(0.0f, 0.0f);
        vertices[v1].uv = math::Vec2(len1, 0.0f);
        vertices[v2].uv = math::Vec2(glm::dot(edge2, right), glm::dot(edge2, up));
    }
    
    // Normalize all UVs to [0, 1] range
    float minU = FLT_MAX, maxU = -FLT_MAX;
    float minV = FLT_MAX, maxV = -FLT_MAX;
    
    for (const auto& vertex : vertices) {
        minU = std::min(minU, vertex.uv.x);
        maxU = std::max(maxU, vertex.uv.x);
        minV = std::min(minV, vertex.uv.y);
        maxV = std::max(maxV, vertex.uv.y);
    }
    
    float rangeU = maxU - minU;
    float rangeV = maxV - minV;
    
    if (rangeU > 0.001f && rangeV > 0.001f) {
        for (auto& vertex : vertices) {
            vertex.uv.x = (vertex.uv.x - minU) / rangeU;
            vertex.uv.y = (vertex.uv.y - minV) / rangeV;
        }
    }
    
}

void MeshTexturePaint::PackUVs(assets::EditableMesh& editable, float padding) {
    // Simplified UV packing: scales UVs to fit in [0, 1] with padding
    // Full implementation would use bin-packing algorithms
    
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Find UV bounds
    float minU = FLT_MAX, maxU = -FLT_MAX;
    float minV = FLT_MAX, maxV = -FLT_MAX;
    
    for (const auto& vertex : vertices) {
        minU = std::min(minU, vertex.uv.x);
        maxU = std::max(maxU, vertex.uv.x);
        minV = std::min(minV, vertex.uv.y);
        maxV = std::max(maxV, vertex.uv.y);
    }
    
    float rangeU = maxU - minU;
    float rangeV = maxV - minV;
    
    if (rangeU > 0.001f && rangeV > 0.001f) {
        // Scale to [padding, 1-padding] range
        float scaleU = (1.0f - 2.0f * padding) / rangeU;
        float scaleV = (1.0f - 2.0f * padding) / rangeV;
        
        for (auto& vertex : vertices) {
            vertex.uv.x = (vertex.uv.x - minU) * scaleU + padding;
            vertex.uv.y = (vertex.uv.y - minV) * scaleV + padding;
        }
    }
    
}

math::AABB MeshTexturePaint::ComputeMeshBounds(const assets::EditableMesh& editable, const Selection& selection) {
    const auto& vertices = editable.GetVertices();
    if (vertices.empty()) {
        return math::AABB();
    }
    
    math::Vec3 min(FLT_MAX), max(-FLT_MAX);
    bool hasAny = false;
    
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        // Use all vertices
        for (const auto& vertex : vertices) {
            min = glm::min(min, vertex.position);
            max = glm::max(max, vertex.position);
            hasAny = true;
        }
    } else {
        // Use selected vertices only
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                min = glm::min(min, vertices[vid].position);
                max = glm::max(max, vertices[vid].position);
                hasAny = true;
            }
        }
    }
    
    if (!hasAny) {
        return math::AABB();
    }
    
    return math::AABB(min, max);
}

void MeshTexturePaint::GetAxisVectors(int axis, math::Vec3& forward, math::Vec3& right, math::Vec3& up) {
    switch (axis) {
        case 0: // X-axis
            forward = math::Vec3(1, 0, 0);
            right = math::Vec3(0, 0, 1);
            up = math::Vec3(0, 1, 0);
            break;
        case 1: // Y-axis
            forward = math::Vec3(0, 1, 0);
            right = math::Vec3(1, 0, 0);
            up = math::Vec3(0, 0, 1);
            break;
        case 2: // Z-axis
            forward = math::Vec3(0, 0, 1);
            right = math::Vec3(1, 0, 0);
            up = math::Vec3(0, 1, 0);
            break;
        default:
            forward = math::Vec3(0, 1, 0);
            right = math::Vec3(1, 0, 0);
            up = math::Vec3(0, 0, 1);
            break;
    }
}

// Texture-Space Painting Implementation

void MeshTexturePaint::PaintTexture(assets::EditableMesh& editable, render::Texture2D& texture,
                                    const Selection& selection, const math::Vec4& color,
                                    float brushSize, float strength, const Mask* mask) {
    if (!texture.IsValid()) return;
    
    const auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    int texWidth = texture.GetWidth();
    int texHeight = texture.GetHeight();
    if (texWidth <= 0 || texHeight <= 0) return;
    
    // Read current texture data
    std::vector<uint8_t> texData = texture.GetData();
    if (texData.empty() || texture.GetFormat() != render::TextureFormat::RGBA8) {
        // Create RGBA8 texture if needed
        std::vector<uint8_t> rgbaData(texWidth * texHeight * 4, 255);
        texture.SetData(rgbaData.data(), 0, 0, texWidth, texHeight);
        texData = texture.GetData();
    }
    
    // Determine which vertices to paint
    std::unordered_set<uint32_t> verticesToPaint;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToPaint.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToPaint.insert(vid);
            }
        }
    }
    
    // Paint each vertex's UV region
    for (uint32_t vid : verticesToPaint) {
        if (vid >= vertices.size()) continue;
        
        const auto& vert = vertices[vid];
        float u = vert.uv.x;
        float v = vert.uv.y;
        
        // Clamp UV to [0, 1]
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        
        // Convert to pixel coordinates
        int px = static_cast<int>(u * texWidth);
        int py = static_cast<int>(v * texHeight);
        px = std::clamp(px, 0, texWidth - 1);
        py = std::clamp(py, 0, texHeight - 1);
        
        // Paint brush area around UV coordinate
        int brushRadius = static_cast<int>(brushSize * std::min(texWidth, texHeight));
        int minX = std::max(0, px - brushRadius);
        int maxX = std::min(texWidth - 1, px + brushRadius);
        int minY = std::max(0, py - brushRadius);
        int maxY = std::min(texHeight - 1, py + brushRadius);
        
        float finalStrength = strength;
        if (mask) {
            finalStrength *= mask->GetWeight(vid);
        }
        
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float dx = (x - px) / static_cast<float>(brushRadius + 1);
                float dy = (y - py) / static_cast<float>(brushRadius + 1);
                float dist = std::sqrt(dx * dx + dy * dy);
                
                if (dist > 1.0f) continue;
                
                // Brush falloff
                float falloff = 1.0f - dist;
                float pixelStrength = finalStrength * falloff;
                
                int idx = (y * texWidth + x) * 4;
                if (idx + 3 >= static_cast<int>(texData.size())) continue;
                
                math::Vec4 currentColor(
                    texData[idx] / 255.0f,
                    texData[idx + 1] / 255.0f,
                    texData[idx + 2] / 255.0f,
                    texData[idx + 3] / 255.0f
                );
                
                math::Vec4 blendedColor(
                    glm::mix(currentColor.r, color.r, pixelStrength),
                    glm::mix(currentColor.g, color.g, pixelStrength),
                    glm::mix(currentColor.b, color.b, pixelStrength),
                    glm::mix(currentColor.a, color.a, pixelStrength)
                );
                
                texData[idx] = static_cast<uint8_t>(blendedColor.r * 255.0f);
                texData[idx + 1] = static_cast<uint8_t>(blendedColor.g * 255.0f);
                texData[idx + 2] = static_cast<uint8_t>(blendedColor.b * 255.0f);
                texData[idx + 3] = static_cast<uint8_t>(blendedColor.a * 255.0f);
            }
        }
    }
    
    // Upload modified texture data
    texture.SetData(texData.data(), 0, 0, texWidth, texHeight);
}

void MeshTexturePaint::FillTexture(assets::EditableMesh& editable, render::Texture2D& texture,
                                   const Selection& selection, const math::Vec4& color) {
    if (!texture.IsValid()) return;
    
    const auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    int texWidth = texture.GetWidth();
    int texHeight = texture.GetHeight();
    if (texWidth <= 0 || texHeight <= 0) return;
    
    // Read current texture data
    std::vector<uint8_t> texData = texture.GetData();
    if (texData.empty() || texture.GetFormat() != render::TextureFormat::RGBA8) {
        std::vector<uint8_t> rgbaData(texWidth * texHeight * 4, 255);
        texture.SetData(rgbaData.data(), 0, 0, texWidth, texHeight);
        texData = texture.GetData();
    }
    
    // Determine which vertices to fill
    std::unordered_set<uint32_t> verticesToFill;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToFill.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToFill.insert(vid);
            }
        }
    }
    
    // Fill pixels covered by UV coordinates
    for (uint32_t vid : verticesToFill) {
        if (vid >= vertices.size()) continue;
        
        const auto& vert = vertices[vid];
        float u = std::clamp(vert.uv.x, 0.0f, 1.0f);
        float v = std::clamp(vert.uv.y, 0.0f, 1.0f);
        
        int px = static_cast<int>(u * texWidth);
        int py = static_cast<int>(v * texHeight);
        px = std::clamp(px, 0, texWidth - 1);
        py = std::clamp(py, 0, texHeight - 1);
        
        int idx = (py * texWidth + px) * 4;
        if (idx + 3 < static_cast<int>(texData.size())) {
            texData[idx] = static_cast<uint8_t>(color.r * 255.0f);
            texData[idx + 1] = static_cast<uint8_t>(color.g * 255.0f);
            texData[idx + 2] = static_cast<uint8_t>(color.b * 255.0f);
            texData[idx + 3] = static_cast<uint8_t>(color.a * 255.0f);
        }
    }
    
    texture.SetData(texData.data(), 0, 0, texWidth, texHeight);
}

void MeshTexturePaint::BlendTexture(assets::EditableMesh& editable, render::Texture2D& texture,
                                   const Selection& selection, const math::Vec4& blendColor,
                                   BlendMode blendMode, float strength, const Mask* mask) {
    if (!texture.IsValid()) return;
    
    const auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    int texWidth = texture.GetWidth();
    int texHeight = texture.GetHeight();
    if (texWidth <= 0 || texHeight <= 0) return;
    
    std::vector<uint8_t> texData = texture.GetData();
    if (texData.empty() || texture.GetFormat() != render::TextureFormat::RGBA8) {
        std::vector<uint8_t> rgbaData(texWidth * texHeight * 4, 255);
        texture.SetData(rgbaData.data(), 0, 0, texWidth, texHeight);
        texData = texture.GetData();
    }
    
    std::unordered_set<uint32_t> verticesToBlend;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToBlend.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToBlend.insert(vid);
            }
        }
    }
    
    for (uint32_t vid : verticesToBlend) {
        if (vid >= vertices.size()) continue;
        
        const auto& vert = vertices[vid];
        float u = std::clamp(vert.uv.x, 0.0f, 1.0f);
        float v = std::clamp(vert.uv.y, 0.0f, 1.0f);
        
        int px = static_cast<int>(u * texWidth);
        int py = static_cast<int>(v * texHeight);
        px = std::clamp(px, 0, texWidth - 1);
        py = std::clamp(py, 0, texHeight - 1);
        
        int idx = (py * texWidth + px) * 4;
        if (idx + 3 >= static_cast<int>(texData.size())) continue;
        
        math::Vec4 currentColor(
            texData[idx] / 255.0f,
            texData[idx + 1] / 255.0f,
            texData[idx + 2] / 255.0f,
            texData[idx + 3] / 255.0f
        );
        
        float finalStrength = strength;
        if (mask) {
            finalStrength *= mask->GetWeight(vid);
        }
        
        math::Vec4 blended = BlendColor(currentColor, blendColor, blendMode, finalStrength);
        
        texData[idx] = static_cast<uint8_t>(blended.r * 255.0f);
        texData[idx + 1] = static_cast<uint8_t>(blended.g * 255.0f);
        texData[idx + 2] = static_cast<uint8_t>(blended.b * 255.0f);
        texData[idx + 3] = static_cast<uint8_t>(blended.a * 255.0f);
    }
    
    texture.SetData(texData.data(), 0, 0, texWidth, texHeight);
}

math::Vec4 MeshTexturePaint::SampleTexture(const render::Texture2D& texture, float u, float v) {
    if (!texture.IsValid()) return math::Vec4(0, 0, 0, 1);
    
    int texWidth = texture.GetWidth();
    int texHeight = texture.GetHeight();
    if (texWidth <= 0 || texHeight <= 0) return math::Vec4(0, 0, 0, 1);
    
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    
    int px = static_cast<int>(u * texWidth);
    int py = static_cast<int>(v * texHeight);
    px = std::clamp(px, 0, texWidth - 1);
    py = std::clamp(py, 0, texHeight - 1);
    
    std::vector<uint8_t> texData = texture.GetData();
    if (texData.empty() || texture.GetFormat() != render::TextureFormat::RGBA8) {
        return math::Vec4(0, 0, 0, 1);
    }
    
    int idx = (py * texWidth + px) * 4;
    if (idx + 3 >= static_cast<int>(texData.size())) {
        return math::Vec4(0, 0, 0, 1);
    }
    
    return math::Vec4(
        texData[idx] / 255.0f,
        texData[idx + 1] / 255.0f,
        texData[idx + 2] / 255.0f,
        texData[idx + 3] / 255.0f
    );
}

void MeshTexturePaint::WriteTexture(render::Texture2D& texture, float u, float v, const math::Vec4& color) {
    if (!texture.IsValid()) return;
    
    int texWidth = texture.GetWidth();
    int texHeight = texture.GetHeight();
    if (texWidth <= 0 || texHeight <= 0) return;
    
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    
    int px = static_cast<int>(u * texWidth);
    int py = static_cast<int>(v * texHeight);
    px = std::clamp(px, 0, texWidth - 1);
    py = std::clamp(py, 0, texHeight - 1);
    
    std::vector<uint8_t> texData = texture.GetData();
    if (texData.empty() || texture.GetFormat() != render::TextureFormat::RGBA8) {
        std::vector<uint8_t> rgbaData(texWidth * texHeight * 4, 255);
        texture.SetData(rgbaData.data(), 0, 0, texWidth, texHeight);
        texData = texture.GetData();
    }
    
    int idx = (py * texWidth + px) * 4;
    if (idx + 3 < static_cast<int>(texData.size())) {
        texData[idx] = static_cast<uint8_t>(color.r * 255.0f);
        texData[idx + 1] = static_cast<uint8_t>(color.g * 255.0f);
        texData[idx + 2] = static_cast<uint8_t>(color.b * 255.0f);
        texData[idx + 3] = static_cast<uint8_t>(color.a * 255.0f);
        
        texture.SetData(texData.data(), px, py, 1, 1);
    }
}

math::Vec4 MeshTexturePaint::BlendColor(const math::Vec4& a, const math::Vec4& b, BlendMode mode, float strength) {
    math::Vec4 result;
    
    switch (mode) {
        case BlendMode::Replace:
            result = glm::mix(a, b, strength);
            break;
            
        case BlendMode::Add:
            result = a + b * strength;
            result.r = std::clamp(result.r, 0.0f, 1.0f);
            result.g = std::clamp(result.g, 0.0f, 1.0f);
            result.b = std::clamp(result.b, 0.0f, 1.0f);
            result.a = std::clamp(result.a, 0.0f, 1.0f);
            break;
            
        case BlendMode::Multiply:
            result = a * (math::Vec4(1.0f) - b * strength + b * strength * a);
            break;
            
        case BlendMode::Subtract:
            result = a - b * strength;
            result.r = std::clamp(result.r, 0.0f, 1.0f);
            result.g = std::clamp(result.g, 0.0f, 1.0f);
            result.b = std::clamp(result.b, 0.0f, 1.0f);
            result.a = std::clamp(result.a, 0.0f, 1.0f);
            break;
            
        case BlendMode::Min:
            result = math::Vec4(
                std::min(a.r, b.r),
                std::min(a.g, b.g),
                std::min(a.b, b.b),
                std::min(a.a, b.a)
            );
            result = glm::mix(a, result, strength);
            break;
            
        case BlendMode::Max:
            result = math::Vec4(
                std::max(a.r, b.r),
                std::max(a.g, b.g),
                std::max(a.b, b.b),
                std::max(a.a, b.a)
            );
            result = glm::mix(a, result, strength);
            break;
    }
    
    return result;
}

} // namespace geometry
} // namespace eyelib
