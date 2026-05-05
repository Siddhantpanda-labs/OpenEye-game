#pragma once

namespace eyelib {
namespace rendering {

/** Scaffold — see documentation/Render_Module/new_render_module.md */
class IUpscaler {
public:
    virtual ~IUpscaler() = default;
};

}  // namespace rendering
}  // namespace eyelib
