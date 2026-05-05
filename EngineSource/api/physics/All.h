#pragma once

#include "api/physics/PhysicsWorld.h"
#include "api/physics/CollisionShape.h"
#include "api/physics/RigidBody.h"
#include "api/physics/Constraint.h"
#include "api/physics/CharacterController.h"
#include "api/physics/Query.h"
#include "api/physics/SoftBody.h"
#include "api/physics/Vehicle.h"

namespace eyelib {
namespace api {
namespace physics {
using ::eyelib::api::physics::PhysicsWorld;
using ::eyelib::api::physics::CollisionShape;
using ::eyelib::api::physics::RigidBody;
using ::eyelib::api::physics::Constraint;
using ::eyelib::api::physics::CharacterController;
using ::eyelib::api::physics::Query;
using ::eyelib::api::physics::SoftBody;
using ::eyelib::api::physics::Vehicle;
using ::eyelib::api::physics::RigidBodyHandle;
using ::eyelib::api::physics::CollisionShapeHandle;
using ::eyelib::api::physics::ConstraintHandle;
using ::eyelib::api::physics::CharacterControllerHandle;
using ::eyelib::api::physics::RaycastResult;
using ::eyelib::api::physics::SweepResult;
using ::eyelib::api::physics::OverlapResult;
} // namespace physics
} // namespace api
} // namespace eyelib

