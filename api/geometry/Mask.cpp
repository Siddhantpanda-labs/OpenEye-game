#include "api/geometry/Mask.h"
#include "api/geometry/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include "api/render/Mesh.h"

namespace eyelib {
namespace api {
namespace geometry {

class Mask::Impl {
public:
    ::eyelib::geometry::Mask mask;
    
    Impl() = default;
    explicit Impl(const ::eyelib::geometry::Mask& m) : mask(m) {}
};

Mask Mask::Create(const Mesh& mesh) {
    auto internalMesh = mesh.GetInternal();
    auto internalMask = ::eyelib::geometry::Mask::Create(*internalMesh);
    Mask result;
    result.m_impl = std::make_unique<Impl>(internalMask);
    return result;
}

Mask Mask::CreateFilled(const Mesh& mesh, float value) {
    auto internalMesh = mesh.GetInternal();
    auto internalMask = ::eyelib::geometry::Mask::CreateFilled(*internalMesh, value);
    Mask result;
    result.m_impl = std::make_unique<Impl>(internalMask);
    return result;
}

Mask::Mask() : m_impl(std::make_unique<Impl>()) {}
Mask::~Mask() = default;

Mask::Mask(const Mask& other) 
    : m_impl(std::make_unique<Impl>(other.m_impl->mask)) {}

Mask& Mask::operator=(const Mask& other) {
    if (this != &other) {
        m_impl->mask = other.m_impl->mask;
    }
    return *this;
}

Mask::Mask(Mask&&) noexcept = default;
Mask& Mask::operator=(Mask&&) noexcept = default;

void Mask::SetWeight(uint32_t vertexID, float weight) {
    m_impl->mask.SetWeight(vertexID, weight);
}

float Mask::GetWeight(uint32_t vertexID) const {
    return m_impl->mask.GetWeight(vertexID);
}

bool Mask::HasWeight(uint32_t vertexID) const {
    return m_impl->mask.HasWeight(vertexID);
}

void Mask::Fill(float value) {
    m_impl->mask.Fill(value);
}

void Mask::Fill(const Selection& selection, float value) {
    m_impl->mask.Fill(selection.GetInternal(), value);
}

void Mask::Paint(const Mesh& mesh, const Selection& selection, float value, float falloff) {
    auto internalMesh = mesh.GetInternal();
    m_impl->mask.Paint(*internalMesh, selection.GetInternal(), value, falloff);
}

void Mask::Blend(const Mask& other, BlendMode mode) {
    auto blendMode = static_cast<::eyelib::geometry::BlendMode>(mode);
    m_impl->mask.Blend(other.m_impl->mask, blendMode);
}

void Mask::Invert() {
    m_impl->mask.Invert();
}

size_t Mask::GetVertexCount() const {
    return m_impl->mask.GetVertexCount();
}

std::vector<float> Mask::GetWeights() const {
    return m_impl->mask.GetWeights();
}

void Mask::Clear() {
    m_impl->mask.Clear();
}

const ::eyelib::geometry::Mask& Mask::GetInternal() const {
    return m_impl->mask;
}

} // namespace geometry
} // namespace api
} // namespace eyelib
