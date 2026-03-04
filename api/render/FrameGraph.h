#pragma once

#include "api/render/TextureTypes.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace api { struct RenderDevice; }
namespace render { class FrameGraph; }
namespace api {

using FGTextureHandle = int;

struct FGTextureDesc {
    unsigned width = 0;
    unsigned height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    bool srgb = true;
    std::string name;
};

class FGPassResources {
public:
    uint32_t GetTexture(FGTextureHandle handle) const;
    unsigned GetWidth() const;
    unsigned GetHeight() const;

private:
    friend class FrameGraph;
    std::function<uint32_t(FGTextureHandle)> m_getTexture;
    unsigned m_width = 0;
    unsigned m_height = 0;
};

class FrameGraph {
public:
    using ExecuteFunc = std::function<void(const FGPassResources&)>;

    FrameGraph();
    ~FrameGraph();

    FrameGraph(const FrameGraph&) = delete;
    FrameGraph& operator=(const FrameGraph&) = delete;
    FrameGraph(FrameGraph&& other) noexcept;
    FrameGraph& operator=(FrameGraph&& other) noexcept;

    FGTextureHandle CreateTexture(const FGTextureDesc& desc);

    int AddPass(const std::string& name, const ExecuteFunc& execute,
                const std::vector<FGTextureHandle>& reads = {},
                const std::vector<FGTextureHandle>& writes = {},
                bool sideEffect = false);

    void Clear();
    /** Compile the graph. Requires non-null device for resource creation. */
    void Compile(RenderDevice device);
    void Execute();

    struct PassInfo { std::string name; double gpuMs = 0.0; bool culled = false; };
    const std::vector<PassInfo>& GetPasses() const;

    uint32_t Resolve(FGTextureHandle handle) const;
    unsigned GetWidth() const;
    unsigned GetHeight() const;
    void SetFrameSize(unsigned w, unsigned h);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
