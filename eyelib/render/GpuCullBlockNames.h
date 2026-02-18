#pragma once

/**
 * SSBO block names for the GPU cull compute shader (cull_indirect.comp).
 * Single source of truth for C++ code; shader must declare blocks with these names.
 */
namespace eyelib {
namespace render {

constexpr const char* kGpuCullBlockIndirectOut = "IndirectOut";
constexpr const char* kGpuCullBlockDrawListIn  = "DrawListIn";

} // namespace render
} // namespace eyelib
