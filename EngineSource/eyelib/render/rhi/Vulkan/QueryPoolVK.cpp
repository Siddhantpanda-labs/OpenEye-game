#include "eyelib/render/rhi/Vulkan/QueryPoolVK.h"
#include "eyelib/render/rhi/Vulkan/DeviceVK.h"

namespace eyelib {
namespace render {
namespace rhi {

QueryPoolVK::QueryPoolVK(DeviceVK* device) : m_device(device) {}

QueryPoolVK::~QueryPoolVK() = default;

QueryType QueryPoolVK::GetType() const { return m_type; }

uint32_t QueryPoolVK::GetCount() const { return m_count; }

uint64_t QueryPoolVK::GetResult(uint32_t index) { (void)index; return 0; }

} // namespace rhi
} // namespace render
} // namespace eyelib
