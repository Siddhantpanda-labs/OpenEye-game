#pragma once

#include "eyelib/render/passes/ShadowPassParams.h"

namespace eyelib {
namespace render {

class ShadowPassSystem {
public:
    void RenderShadowBatched(const ShadowPassBatchedParams& p) const;
};

}  // namespace render
}  // namespace eyelib
