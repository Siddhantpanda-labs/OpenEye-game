#include "eyelib/render/rhi/diligent/QueryPoolDiligent.h"

namespace eyelib {
namespace render {
namespace rhi {

QueryType QueryPoolDiligent::GetType() const { return m_type; }
uint32_t QueryPoolDiligent::GetCount() const { return m_count; }
uint64_t QueryPoolDiligent::GetResult(uint32_t index) {
    (void)index;
    return 0;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
