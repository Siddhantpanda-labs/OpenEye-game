#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct QueryPoolGL : IQueryPool {
    QueryType GetType() const override;
    uint32_t GetCount() const override;
    uint64_t GetResult(uint32_t index) override;

    std::vector<unsigned int> m_queryIds;  // GL query IDs (one per slot for timestamp/elapsed)
    QueryType m_type = QueryType::Timestamp;
    uint32_t m_count = 0;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
