#pragma once

/**
 * @brief Convenience header that includes all Physics API headers
 * 
 * Apps can include this single header to get all Physics functionality:
 * #include "physics/api/All.h"
 */

#include "PhysicsWorld.h"

// Note: PhysicsWorld API hides all Bullet dependencies
// Apps should use API::PhysicsWorld, not Physics::PhysicsWorld directly

