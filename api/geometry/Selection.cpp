#include "api/geometry/Selection.h"
#include "eyelib/geometry/selection/Selection.h"
#include "api/render/Mesh.h"

namespace eyelib {
namespace api {
namespace geometry {

class Selection::Impl {
public:
    ::eyelib::geometry::Selection selection;
    
    Impl() = default;
    explicit Impl(const ::eyelib::geometry::Selection& sel) : selection(sel) {}
};

Selection::Selection() : m_impl(std::make_unique<Impl>()) {}
Selection::~Selection() = default;

Selection::Selection(const Selection& other) 
    : m_impl(std::make_unique<Impl>(other.m_impl->selection)) {}

Selection& Selection::operator=(const Selection& other) {
    if (this != &other) {
        m_impl->selection = other.m_impl->selection;
    }
    return *this;
}

Selection::Selection(Selection&&) noexcept = default;
Selection& Selection::operator=(Selection&&) noexcept = default;

Selection::Selection(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}

Selection::Type Selection::GetType() const {
    auto type = m_impl->selection.GetType();
    return static_cast<Selection::Type>(type);
}

bool Selection::IsEmpty() const {
    return m_impl->selection.IsEmpty();
}

size_t Selection::GetCount() const {
    return m_impl->selection.GetCount();
}

bool Selection::Contains(uint32_t id) const {
    return m_impl->selection.Contains(id);
}

std::vector<uint32_t> Selection::GetIDs() const {
    return m_impl->selection.GetIDs();
}

Selection Selection::Union(const Selection& other) const {
    auto result = m_impl->selection.Union(other.m_impl->selection);
    return Selection(std::make_unique<Impl>(result));
}

Selection Selection::Intersect(const Selection& other) const {
    auto result = m_impl->selection.Intersect(other.m_impl->selection);
    return Selection(std::make_unique<Impl>(result));
}

Selection Selection::Subtract(const Selection& other) const {
    auto result = m_impl->selection.Subtract(other.m_impl->selection);
    return Selection(std::make_unique<Impl>(result));
}

const ::eyelib::geometry::Selection& Selection::GetInternal() const {
    return m_impl->selection;
}

// SelectionBuilder implementation

class SelectionBuilder::Impl {
public:
    ::eyelib::geometry::SelectionBuilder builder;
    
    explicit Impl(const ::eyelib::render::Mesh& mesh) : builder(mesh) {}
};

SelectionBuilder::SelectionBuilder(const Mesh& mesh) 
    : m_impl(std::make_unique<Impl>(*mesh.GetInternal())) {}

SelectionBuilder::~SelectionBuilder() = default;

SelectionBuilder& SelectionBuilder::ByVertexIDs(const std::vector<uint32_t>& vertexIDs) {
    m_impl->builder.ByVertexIDs(vertexIDs);
    return *this;
}

SelectionBuilder& SelectionBuilder::ByEdgeIDs(const std::vector<uint32_t>& edgeIDs) {
    m_impl->builder.ByEdgeIDs(edgeIDs);
    return *this;
}

SelectionBuilder& SelectionBuilder::ByFaceIDs(const std::vector<uint32_t>& faceIDs) {
    m_impl->builder.ByFaceIDs(faceIDs);
    return *this;
}

SelectionBuilder& SelectionBuilder::ByBounds(const ::eyelib::math::AABB& bounds) {
    m_impl->builder.ByBounds(bounds);
    return *this;
}

SelectionBuilder& SelectionBuilder::ByNormal(const ::eyelib::math::Vec3& direction, float threshold) {
    m_impl->builder.ByNormal(direction, threshold);
    return *this;
}

SelectionBuilder& SelectionBuilder::ByDistance(const ::eyelib::math::Vec3& point, float radius) {
    m_impl->builder.ByDistance(point, radius);
    return *this;
}

SelectionBuilder& SelectionBuilder::Union(const Selection& other) {
    m_impl->builder.Union(other.m_impl->selection);
    return *this;
}

SelectionBuilder& SelectionBuilder::Intersect(const Selection& other) {
    m_impl->builder.Intersect(other.m_impl->selection);
    return *this;
}

Selection SelectionBuilder::Build() const {
    auto result = m_impl->builder.Build();
    return Selection(std::make_unique<Selection::Impl>(result));
}

} // namespace geometry
} // namespace api
} // namespace eyelib
