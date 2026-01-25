#pragma once

#include <cstdint>

namespace eyelib {
namespace ecs {

using Entity = uint32_t;
static constexpr Entity NullEntity = 0xFFFFFFFFu;

} // namespace ecs
} // namespace eyelib
