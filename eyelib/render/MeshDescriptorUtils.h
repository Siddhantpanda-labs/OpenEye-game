#pragma once

#include "eyelib/math/AABB.h"

namespace eyelib {
namespace render {

struct MeshDescriptor;

/** CPU-only mesh helpers (Phase 2.4). Operate on MeshDescriptor; use before CreateFromDescriptor. */
namespace MeshDescriptorUtils {

void GenerateNormals(MeshDescriptor& desc);
void GenerateTangents(MeshDescriptor& desc);
void Optimize(MeshDescriptor& desc);
math::AABB ComputeAABB(const MeshDescriptor& desc);

} // namespace MeshDescriptorUtils
} // namespace render
} // namespace eyelib
