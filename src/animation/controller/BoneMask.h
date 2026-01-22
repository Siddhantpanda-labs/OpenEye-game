#pragma once

#include <string>
#include <vector>

namespace OpenEye {
namespace Animation {

class Skeleton;

namespace Controller {

struct BoneMask {
    std::string name;
    // If empty => full-body mask (all weights 1)
    std::vector<std::string> includedBoneNames;
};

struct CompiledBoneMask {
    std::string name;
    std::vector<float> weights; // per-bone [0..1]
};

class BoneMaskCompiler {
public:
    static CompiledBoneMask Compile(const BoneMask& mask, const Skeleton& skeleton);
};

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

