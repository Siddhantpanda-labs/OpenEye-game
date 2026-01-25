#pragma once

#include "Entity.h"
#include "Components.h"
#include "Component.h"
#include "Query.h"
#include "QueryResult.h"
#include "System.h"
#include "World.h"

namespace eyelib {
namespace api {
namespace ecs {
using ::eyelib::api::Entity;
using ::eyelib::api::World;
using ::eyelib::api::Query;
using ::eyelib::api::QueryResult;
using ::eyelib::api::SystemManager;
using ::eyelib::api::Transform;
using ::eyelib::api::Name;
using ::eyelib::api::Renderable;
using ::eyelib::api::Camera;
using ::eyelib::api::Light;
using ::eyelib::api::RigidBody;
using ::eyelib::api::AnimatorComponent;
using ::eyelib::api::AABB;
} // namespace ecs
} // namespace api
} // namespace eyelib

