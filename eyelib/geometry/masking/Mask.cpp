#include "eyelib/geometry/masking/Mask.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include <algorithm>
#include <cmath>

namespace eyelib {
namespace geometry {

Mask Mask::Create(const render::Mesh& mesh) {
    return Mask(mesh.GetVertexCount());
}

Mask Mask::CreateFilled(const render::Mesh& mesh, float value) {
    Mask mask(mesh.GetVertexCount());
    mask.Fill(value);
    return mask;
}

Mask::Mask() : m_vertexCount(0) {}

Mask::Mask(size_t vertexCount) : m_vertexCount(vertexCount) {
    m_weights.resize(vertexCount, 0.0f);
}

void Mask::SetWeight(uint32_t vertexID, float weight) {
    if (vertexID < m_weights.size()) {
        m_weights[vertexID] = ClampWeight(weight);
    }
}

float Mask::GetWeight(uint32_t vertexID) const {
    if (vertexID < m_weights.size()) {
        return m_weights[vertexID];
    }
    return 0.0f;
}

bool Mask::HasWeight(uint32_t vertexID) const {
    return vertexID < m_weights.size() && m_weights[vertexID] > 0.0f;
}

void Mask::Fill(float value) {
    float clamped = ClampWeight(value);
    std::fill(m_weights.begin(), m_weights.end(), clamped);
}

void Mask::Fill(const Selection& selection, float value) {
    if (selection.GetType() != Selection::Type::Vertices) {
        return; // Only works with vertex selection
    }
    
    float clamped = ClampWeight(value);
    for (uint32_t vertexID : selection.GetIDs()) {
        if (vertexID < m_weights.size()) {
            m_weights[vertexID] = clamped;
        }
    }
}

void Mask::Paint(const render::Mesh& mesh, const Selection& selection, float value, float falloff) {
    if (selection.GetType() != Selection::Type::Vertices) {
        return;
    }
    
    const auto& vertices = mesh.GetVertices();
    float clampedValue = ClampWeight(value);
    
    if (falloff <= 0.0f) {
        // Hard edge - just fill selection
        Fill(selection, clampedValue);
        return;
    }
    
    // Soft falloff - compute distance-based weights
    // For simplicity, use center of selection as falloff point
    math::Vec3 center(0.0f);
    size_t count = 0;
    for (uint32_t vertexID : selection.GetIDs()) {
        if (vertexID < vertices.size()) {
            center += vertices[vertexID].position;
            count++;
        }
    }
    if (count > 0) {
        center /= static_cast<float>(count);
    }
    
    // Apply falloff
    for (uint32_t vertexID : selection.GetIDs()) {
        if (vertexID < vertices.size() && vertexID < m_weights.size()) {
            math::Vec3 diff = vertices[vertexID].position - center;
            float distance = glm::length(diff);
            float t = std::max(0.0f, 1.0f - (distance / falloff));
            float currentWeight = m_weights[vertexID];
            m_weights[vertexID] = ClampWeight(currentWeight + t * clampedValue);
        }
    }
}

void Mask::Blend(const Mask& other, BlendMode mode) {
    size_t minSize = std::min(m_weights.size(), other.m_weights.size());
    
    for (size_t i = 0; i < minSize; ++i) {
        float a = m_weights[i];
        float b = other.m_weights[i];
        float result = 0.0f;
        
        switch (mode) {
            case BlendMode::Replace:
                result = b;
                break;
            case BlendMode::Add:
                result = ClampWeight(a + b);
                break;
            case BlendMode::Multiply:
                result = a * b;
                break;
            case BlendMode::Subtract:
                result = ClampWeight(a - b);
                break;
            case BlendMode::Min:
                result = std::min(a, b);
                break;
            case BlendMode::Max:
                result = std::max(a, b);
                break;
        }
        
        m_weights[i] = result;
    }
}

void Mask::Invert() {
    for (float& weight : m_weights) {
        weight = 1.0f - weight;
    }
}

void Mask::Clear() {
    std::fill(m_weights.begin(), m_weights.end(), 0.0f);
}

float Mask::ClampWeight(float weight) const {
    return std::max(0.0f, std::min(1.0f, weight));
}

} // namespace geometry
} // namespace eyelib
