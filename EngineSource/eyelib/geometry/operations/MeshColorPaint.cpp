#include "eyelib/geometry/operations/MeshColorPaint.h"
#include "eyelib/geometry/fields/NoiseField.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <unordered_set>
#include <cmath>

namespace eyelib {
namespace geometry {

void MeshColorPaint::PaintVertexColor(assets::EditableMesh& editable, const Selection& selection,
                                     const math::Vec4& color, float strength,
                                     float falloff, const Mask* mask) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
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
    
    // Compute falloff center if needed
    math::Vec3 falloffCenter(0.0f);
    if (falloff > 0.0f && !selection.IsEmpty()) {
        falloffCenter = ComputeSelectionCenter(editable, selection);
    }
    
    // Paint vertices
    for (uint32_t vid : verticesToPaint) {
        if (vid >= vertices.size()) continue;
        
        float finalStrength = strength;
        
        // Apply falloff if specified
        if (falloff > 0.0f) {
            math::Vec3 diff = vertices[vid].position - falloffCenter;
            float distance = glm::length(diff);
            float t = std::max(0.0f, 1.0f - (distance / falloff));
            finalStrength *= t;
        }
        
        // Apply mask if provided
        if (mask) {
            finalStrength *= mask->GetWeight(vid);
        }
        
        // Blend color
        math::Vec4& currentColor = vertices[vid].color;
        currentColor = math::Vec4(
            glm::mix(currentColor.r, color.r, finalStrength),
            glm::mix(currentColor.g, color.g, finalStrength),
            glm::mix(currentColor.b, color.b, finalStrength),
            glm::mix(currentColor.a, color.a, finalStrength)
        );
    }
    
}

void MeshColorPaint::FillColor(assets::EditableMesh& editable, const Selection& selection,
                               const math::Vec4& color) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
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
    
    // Fill colors
    for (uint32_t vid : verticesToFill) {
        if (vid >= vertices.size()) continue;
        vertices[vid].color = color;
    }
    
}

void MeshColorPaint::GradientColor(assets::EditableMesh& editable, const Selection& selection,
                                  const math::Vec4& startColor, const math::Vec4& endColor,
                                  const math::Vec3& direction, const Mask* mask) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    math::Vec3 normalizedDir = glm::normalize(direction);
    
    // Compute bounds for gradient range
    math::AABB bounds;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        bounds = math::AABB(vertices.front().position, vertices.front().position);
        for (size_t i = 1; i < vertices.size(); ++i) bounds.Expand(vertices[i].position);
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                bounds.Expand(vertices[vid].position);
            }
        }
    }
    
    math::Vec3 minPos = bounds.min;
    math::Vec3 maxPos = bounds.max;
    
    // Project bounds onto gradient direction
    float minProj = glm::dot(minPos, normalizedDir);
    float maxProj = glm::dot(maxPos, normalizedDir);
    float range = maxProj - minProj;
    if (range < 0.001f) range = 1.0f;
    
    // Determine which vertices to apply gradient to
    std::unordered_set<uint32_t> verticesToGradient;
    if (selection.IsEmpty() || selection.GetType() != Selection::Type::Vertices) {
        for (size_t i = 0; i < vertices.size(); ++i) {
            verticesToGradient.insert(static_cast<uint32_t>(i));
        }
    } else {
        for (uint32_t vid : selection.GetIDs()) {
            if (vid < vertices.size()) {
                verticesToGradient.insert(vid);
            }
        }
    }
    
    // Apply gradient
    for (uint32_t vid : verticesToGradient) {
        if (vid >= vertices.size()) continue;
        
        float proj = glm::dot(vertices[vid].position, normalizedDir);
        float t = (proj - minProj) / range;
        t = std::clamp(t, 0.0f, 1.0f);
        
        math::Vec4 gradientColor(
            glm::mix(startColor.r, endColor.r, t),
            glm::mix(startColor.g, endColor.g, t),
            glm::mix(startColor.b, endColor.b, t),
            glm::mix(startColor.a, endColor.a, t)
        );
        
        float finalStrength = 1.0f;
        if (mask) {
            finalStrength = mask->GetWeight(vid);
        }
        
        math::Vec4& currentColor = vertices[vid].color;
        currentColor = math::Vec4(
            glm::mix(currentColor.r, gradientColor.r, finalStrength),
            glm::mix(currentColor.g, gradientColor.g, finalStrength),
            glm::mix(currentColor.b, gradientColor.b, finalStrength),
            glm::mix(currentColor.a, gradientColor.a, finalStrength)
        );
    }
    
}

void MeshColorPaint::NoisePaint(assets::EditableMesh& editable, const Selection& selection,
                               const math::Vec4& baseColor, const math::Vec4& noiseColor,
                               float scale, float strength, uint32_t seed,
                               const Mask* mask) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Create noise field
    auto noiseFunc = NoiseField::Create(NoiseType::FBM, scale, seed, 4, 0.5f);
    
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
    
    // Apply noise painting
    for (uint32_t vid : verticesToPaint) {
        if (vid >= vertices.size()) continue;
        
        float noiseValue = noiseFunc(vertices[vid].position);
        // Normalize noise from [-1, 1] to [0, 1]
        float normalizedNoise = (noiseValue + 1.0f) * 0.5f;
        
        math::Vec4 noiseBlendedColor(
            glm::mix(baseColor.r, noiseColor.r, normalizedNoise),
            glm::mix(baseColor.g, noiseColor.g, normalizedNoise),
            glm::mix(baseColor.b, noiseColor.b, normalizedNoise),
            glm::mix(baseColor.a, noiseColor.a, normalizedNoise)
        );
        
        float finalStrength = strength;
        if (mask) {
            finalStrength *= mask->GetWeight(vid);
        }
        
        math::Vec4& currentColor = vertices[vid].color;
        currentColor = math::Vec4(
            glm::mix(currentColor.r, noiseBlendedColor.r, finalStrength),
            glm::mix(currentColor.g, noiseBlendedColor.g, finalStrength),
            glm::mix(currentColor.b, noiseBlendedColor.b, finalStrength),
            glm::mix(currentColor.a, noiseBlendedColor.a, finalStrength)
        );
    }
    
}

void MeshColorPaint::BlendColors(assets::EditableMesh& editable, const Selection& selection,
                                const math::Vec4& blendColor, BlendMode blendMode,
                                float strength, const Mask* mask) {
    auto& vertices = editable.GetVertices();
    if (vertices.empty()) return;
    
    // Determine which vertices to blend
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
    
    // Blend colors
    for (uint32_t vid : verticesToBlend) {
        if (vid >= vertices.size()) continue;
        
        float finalStrength = strength;
        if (mask) {
            finalStrength *= mask->GetWeight(vid);
        }
        
        math::Vec4& currentColor = vertices[vid].color;
        currentColor = BlendColor(currentColor, blendColor, blendMode, finalStrength);
    }
    
}

void MeshColorPaint::SampleColor(const assets::EditableMesh& source, assets::EditableMesh& target,
                                const Selection& sourceSelection, const Selection& targetSelection) {
    const auto& sourceVertices = source.GetVertices();
    auto& targetVertices = target.GetVertices();
    
    if (sourceVertices.empty() || targetVertices.empty()) return;
    if (sourceSelection.IsEmpty() || targetSelection.IsEmpty()) return;
    if (sourceSelection.GetType() != Selection::Type::Vertices ||
        targetSelection.GetType() != Selection::Type::Vertices) {
        return;
    }
    
    const auto& sourceIDs = sourceSelection.GetIDs();
    const auto& targetIDs = targetSelection.GetIDs();
    
    // Simple 1:1 mapping if counts match, otherwise use first source color
    if (sourceIDs.size() == targetIDs.size()) {
        for (size_t i = 0; i < sourceIDs.size() && i < targetIDs.size(); ++i) {
            uint32_t srcID = sourceIDs[i];
            uint32_t tgtID = targetIDs[i];
            
            if (srcID < sourceVertices.size() && tgtID < targetVertices.size()) {
                targetVertices[tgtID].color = sourceVertices[srcID].color;
            }
        }
    } else {
        // Use first source color for all targets
        math::Vec4 sampleColor(1, 1, 1, 1);
        if (!sourceIDs.empty() && sourceIDs[0] < sourceVertices.size()) {
            sampleColor = sourceVertices[sourceIDs[0]].color;
        }
        
        for (uint32_t tgtID : targetIDs) {
            if (tgtID < targetVertices.size()) {
                targetVertices[tgtID].color = sampleColor;
            }
        }
    }
    
}

math::Vec3 MeshColorPaint::ComputeSelectionCenter(const assets::EditableMesh& editable, const Selection& selection) {
    const auto& vertices = editable.GetVertices();
    math::Vec3 center(0.0f);
    size_t count = 0;
    
    for (uint32_t vid : selection.GetIDs()) {
        if (vid < vertices.size()) {
            center += vertices[vid].position;
            count++;
        }
    }
    
    if (count > 0) {
        center /= static_cast<float>(count);
    }
    
    return center;
}

math::Vec4 MeshColorPaint::BlendColor(const math::Vec4& a, const math::Vec4& b, BlendMode mode, float strength) {
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
