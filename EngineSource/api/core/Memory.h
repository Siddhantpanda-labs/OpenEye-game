#pragma once

/**
 * @brief Memory utilities API (alignment, aligned alloc/free, copy/move/set).
 * Re-exports eyelib::core facilities.
 */
#include "eyelib/core/Memory.h"

namespace api {
namespace memory {

using eyelib::core::AlignUp;
using eyelib::core::AlignDown;
using eyelib::core::IsAligned;
using eyelib::core::AllocateAligned;
using eyelib::core::FreeAligned;
using eyelib::core::MemoryCopy;
using eyelib::core::MemoryMove;
using eyelib::core::MemorySet;

} // namespace memory
} // namespace api
