#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

struct FGTextureDesc {
    unsigned width = 0;
    unsigned height = 0;
    rhi::RHITextureFormat format = rhi::RHITextureFormat::RGBA8;
    bool srgb = true;
    std::string name;
};

using FGTextureHandle = int;  // -1 invalid

struct FGPassResources {
    std::function<uint32_t(FGTextureHandle)> getTexture;
    std::function<rhi::ITexture*(FGTextureHandle)> getTextureRHI;
    unsigned width = 0;
    unsigned height = 0;
};

class FrameGraph {
public:
    FrameGraph();
    ~FrameGraph();

    /** Requires SetDevice() before Compile(). */
    FGTextureHandle CreateTexture(const FGTextureDesc& desc);
    /** Legacy: import backend-native texture (e.g. GL texture id). */
    FGTextureHandle ImportTexture(const std::string& name, uint32_t nativeTextureId, unsigned width, unsigned height);
    FGTextureHandle ImportTextureRHI(const std::string& name, rhi::ITexture* texture, unsigned width, unsigned height);

    void SetDevice(rhi::IDevice* device) { m_device = device; }

    using ExecuteFunc = std::function<void(const FGPassResources&)>;

    int AddPass(const std::string& name, const ExecuteFunc& execute,
                const std::vector<FGTextureHandle>& reads = {},
                const std::vector<FGTextureHandle>& writes = {},
                bool sideEffect = false);

    void Clear();
    void Compile();
    /** Pass command list for pass timing (BeginQuery/EndQuery). If null, passes run without timing. */
    void Execute(rhi::ICommandList* cmd = nullptr);

    struct PassInfo { std::string name; double gpuMs = 0.0; bool culled = false; };
    const std::vector<PassInfo>& GetPasses() const { return m_passInfos; }

    /** Backend-native texture id (e.g. GL name); 0 if handle invalid or RHI-only. */
    uint32_t Resolve(FGTextureHandle h) const;
    rhi::ITexture* ResolveRHI(FGTextureHandle h) const;
    unsigned GetWidth() const { return m_width; }
    unsigned GetHeight() const { return m_height; }
    void SetFrameSize(unsigned w, unsigned h) { m_width = w; m_height = h; }

private:
    struct TextureNode {
        FGTextureDesc desc;
        uint32_t nativeId = 0;  // legacy imported (e.g. GL texture id)
        rhi::ITexture* rhiTexture = nullptr;
        bool imported = false;
        unsigned width = 0;
        unsigned height = 0;
    };
    struct PassNode {
        std::string name;
        ExecuteFunc execute;
        std::vector<FGTextureHandle> reads, writes;
        bool sideEffect = false;
        rhi::IQueryPool* queryPool = nullptr;
        bool culled = false;
    };

    FGTextureHandle AllocHandle();
    void CreateTextureRHI(TextureNode& n);

    rhi::IDevice* m_device = nullptr;
    std::vector<TextureNode> m_textures;
    std::vector<PassNode> m_passes;
    std::vector<PassInfo> m_passInfos;
    unsigned m_width = 0, m_height = 0;
};

} // namespace render
} // namespace eyelib
