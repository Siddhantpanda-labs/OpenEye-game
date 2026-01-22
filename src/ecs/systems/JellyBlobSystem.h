#pragma once

#include "../Scene.h"

namespace OpenEye {
namespace ECS {

class JellyBlobSystem {
public:
    static void Update(Scene& scene, float deltaTime);
};

} // namespace ECS
} // namespace OpenEye

