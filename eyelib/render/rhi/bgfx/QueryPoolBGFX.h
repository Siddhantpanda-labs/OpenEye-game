#pragma once

#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
namespace rhi {

struct QueryPoolBGFX : IQueryPool {
    QueryType GetType() const override;
    uint32_t GetCount() const override;
    uint64_t GetResult(uint32_t index) override;

    QueryType m_type = QueryType::Timestamp;
    uint32_t m_count = 0;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
