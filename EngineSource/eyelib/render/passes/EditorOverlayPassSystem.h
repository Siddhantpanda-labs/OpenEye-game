#pragma once

#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <functional>

namespace eyelib {
namespace render {

class Shader;

struct OutlineOverlayBuildParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    rhi::IRenderTarget* hdrRenderTarget = nullptr;
    rhi::IRenderTarget* entityIdRenderTarget = nullptr;
    rhi::IRenderTarget* outlineCompositeTemp = nullptr;
    Shader* outlineShader = nullptr;
    rhi::IDescriptorSet* outlineSet = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t selectedPackedId = 0;
    float colorR = 1.0f;
    float colorG = 0.5f;
    float colorB = 0.0f;
    bool useTypedOutlinePush = false;
    std::function<void(rhi::ICommandList*, rhi::IRenderTarget*, rhi::IRenderTarget*, rhi::IDescriptorSet*)> bindDescriptorSet;
    std::function<void(rhi::ICommandList*, rhi::IRenderTarget*, rhi::IRenderTarget*)> bindFallbackTextures;
    std::function<void(rhi::ICommandList*, Shader*, bool, uint32_t, float, float, float, float, float)> writePush;
};

struct OutlineOverlayExecuteParams {
    OutlineOverlayBuildParams build{};
};

struct EntityIdOverlayBuildParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    rhi::IRenderTarget* entityIdRenderTarget = nullptr;
    Shader* entityIdShader = nullptr;
    CameraView camView{};
    uint32_t width = 0;
    uint32_t height = 0;
    bool useTypedPush = false;
    std::function<void(rhi::ICommandList*, Shader*, bool)> drawItems;
};

struct EntityIdOverlayExecuteParams {
    EntityIdOverlayBuildParams build{};
};

struct DebugLinesOverlayBuildParams {
    rhi::IDevice* device = nullptr;
    rhi::ICommandList* cmd = nullptr;
    rhi::IRenderTarget* renderTarget = nullptr;
    math::Mat4 view = math::Mat4(1.0f);
    math::Mat4 projection = math::Mat4(1.0f);
    uint32_t width = 0;
    uint32_t height = 0;
    bool hasAnyLines = false;
    std::function<void(rhi::ICommandList*, rhi::IRenderTarget*, const CameraView&, uint32_t, uint32_t)> drawLines;
};

struct DebugLinesOverlayExecuteParams {
    DebugLinesOverlayBuildParams build{};
    CameraView camView{};
};

class EditorOverlayPassSystem {
public:
    OutlineOverlayExecuteParams BuildOutlineParams(const OutlineOverlayBuildParams& params) const;
    bool ExecuteOutline(const OutlineOverlayExecuteParams& params) const;

    EntityIdOverlayExecuteParams BuildEntityIdParams(const EntityIdOverlayBuildParams& params) const;
    bool ExecuteEntityId(const EntityIdOverlayExecuteParams& params) const;

    DebugLinesOverlayExecuteParams BuildDebugLinesParams(const DebugLinesOverlayBuildParams& params) const;
    bool ExecuteDebugLines(const DebugLinesOverlayExecuteParams& params) const;
};

}  // namespace render
}  // namespace eyelib

