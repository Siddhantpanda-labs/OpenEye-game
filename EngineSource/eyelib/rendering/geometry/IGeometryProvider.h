#pragma once

namespace eyelib {
namespace rendering {

/** Scaffold — see documentation/Render_Module/new_render_module.md */
class IGeometryProvider {
public:
    virtual ~IGeometryProvider() = default;
};

}  // namespace rendering
}  // namespace eyelib
