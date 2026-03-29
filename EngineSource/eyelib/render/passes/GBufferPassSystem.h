#pragma once

#include "eyelib/render/passes/GBufferPassOpaqueParams.h"

namespace eyelib {
namespace render {

class GBufferPassSystem {
public:
    void RenderOpaqueGeometry(const GBufferPassOpaqueParams& p) const;
};

}  // namespace render
}  // namespace eyelib
