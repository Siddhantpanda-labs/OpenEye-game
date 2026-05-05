#pragma once

namespace eyelib {
namespace rendering {

/** Scaffold — see documentation/Render_Module/new_render_module.md */
class IShadowStrategy {
public:
    virtual ~IShadowStrategy() = default;
};

}  // namespace rendering
}  // namespace eyelib
