#pragma once

namespace eyelib {
namespace rendering {

/** Scaffold — see documentation/Render_Module/new_render_module.md */
class IReflectionStrategy {
public:
    virtual ~IReflectionStrategy() = default;
};

}  // namespace rendering
}  // namespace eyelib
