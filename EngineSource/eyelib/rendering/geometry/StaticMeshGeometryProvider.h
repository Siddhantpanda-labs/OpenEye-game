#pragma once

#include "eyelib/rendering/geometry/IGeometryProvider.h"

namespace eyelib {
namespace rendering {

class IMeshHandleResolver;

/** Default provider: resolves `meshHandle` via `IMeshHandleResolver`, binds VB/IB, issues `DrawIndexed` per item. */
class StaticMeshGeometryProvider final : public IGeometryProvider {
public:
    explicit StaticMeshGeometryProvider(const IMeshHandleResolver& resolver) noexcept : m_resolver(&resolver) {}

    void Submit(rhi::ICommandList* cmd, const DrawList& drawList, uint32_t beginIndex, uint32_t count) const override;

private:
    const IMeshHandleResolver* m_resolver = nullptr;
};

}  // namespace rendering
}  // namespace eyelib
