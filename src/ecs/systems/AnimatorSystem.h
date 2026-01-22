#pragma once

#include "../Scene.h"

namespace OpenEye {
namespace ECS {

class AnimatorSystem {
public:
    static void Update(Scene& scene, float deltaTime, void* physicsWorld = nullptr);
};

} // namespace ECS
} // namespace OpenEye

