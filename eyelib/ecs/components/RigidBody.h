#pragma once

#include "eyelib/physics/PhysicsWorld.h"
#include "eyelib/physics/shapes/CollisionShapeFactory.h"

namespace eyelib {
namespace ecs {
namespace components {

struct RigidBody {
    physics::RigidBodyHandle bodyHandle = 0;
    physics::CollisionShapeHandle shapeHandle = 0;
    bool syncTransform = true;
};

} // namespace components
} // namespace ecs
} // namespace eyelib
