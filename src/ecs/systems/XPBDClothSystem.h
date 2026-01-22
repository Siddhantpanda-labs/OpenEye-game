#pragma once

#include "../Scene.h"

namespace OpenEye {
namespace ECS {

// CPU XPBD cloth simulation (intended as the "most cloth" solution: scalable + predictable)
class XPBDClothSystem {
public:
    static void Update(Scene& scene, float deltaTime);
};

} // namespace ECS
} // namespace OpenEye

