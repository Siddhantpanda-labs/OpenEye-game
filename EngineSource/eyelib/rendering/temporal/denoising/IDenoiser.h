#pragma once

namespace eyelib {
namespace rendering {

/** Scaffold — see documentation/Render_Module/new_render_module.md */
class IDenoiser {
public:
    virtual ~IDenoiser() = default;
};

}  // namespace rendering
}  // namespace eyelib
