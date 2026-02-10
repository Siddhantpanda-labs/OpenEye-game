#include "eyelib/render/rhi/bgfx/QueryPoolBGFX.h"

namespace eyelib {
namespace render {
namespace rhi {

QueryType QueryPoolBGFX::GetType() const { return m_type; }
uint32_t QueryPoolBGFX::GetCount() const { return m_count; }

uint64_t QueryPoolBGFX::GetResult(uint32_t index) {
    (void)index;
    return 0;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
