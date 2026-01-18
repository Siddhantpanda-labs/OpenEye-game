#pragma once

#include "../resources/Mesh.h"

namespace OpenEye {
namespace Rendering {
namespace API {

/**
 * @brief Public API for Mesh
 * 
 * Re-exports Mesh from the Rendering component.
 */
using Mesh = OpenEye::Rendering::Mesh;
using Vertex = OpenEye::Rendering::Vertex;

} // namespace API
} // namespace Rendering
} // namespace OpenEye

