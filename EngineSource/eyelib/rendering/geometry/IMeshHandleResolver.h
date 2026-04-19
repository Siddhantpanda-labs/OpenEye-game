#pragma once

#include "eyelib/rendering/resources/Mesh.h"

#include <cstdint>

namespace eyelib {
namespace rendering {

/**
 * Maps abstract `DrawItem::meshHandle` to uploaded GPU geometry.
 * Sandbox / content layers provide concrete implementations; geometry providers stay lighting-agnostic (doc §6).
 */
class IMeshHandleResolver {
public:
    virtual ~IMeshHandleResolver() = default;

    /** Invalid / unknown handles return default-constructed `MeshHandle` (`IsValid() == false`). */
    virtual MeshHandle TryResolveMesh(uint32_t meshHandle) const noexcept = 0;
};

}  // namespace rendering
}  // namespace eyelib
