#pragma once

#include <memory>

namespace eyelib {
namespace render {
class Mesh;
class Material;
}
namespace animation {
class Animator;
class Skeleton;
}
namespace ecs {

using MeshHandle = std::shared_ptr<render::Mesh>;
using MaterialHandle = std::shared_ptr<render::Material>;
using AnimatorHandle = std::shared_ptr<animation::Animator>;
using SkeletonHandle = std::shared_ptr<animation::Skeleton>;

} // namespace ecs
} // namespace eyelib
