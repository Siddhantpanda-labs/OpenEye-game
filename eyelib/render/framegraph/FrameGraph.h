#pragma once

#include "eyelib/render/rhi/RHI.h"
#include <functional>
#include <string>
#include <vector>
#include <glad/glad.h>

namespace eyelib {
namespace render {

struct FGTextureDesc {
    unsigned width = 0;
    unsigned height = 0;
    GLenum internalFormat = GL_SRGB8_ALPHA8;
    bool srgb = true;
    std::string name;
};

using FGTextureHandle = int;  // -1 invalid

struct FGPassResources {
    std::function<GLuint(FGTextureHandle)> getTexture;
    std::function<rhi::ITexture*(FGTextureHandle)> getTextureRHI;
    unsigned width = 0;
    unsigned height = 0;
};

class FrameGraph {
public:
    FrameGraph();
    ~FrameGraph();

    FGTextureHandle CreateTexture(const FGTextureDesc& desc);
    FGTextureHandle ImportTexture(const std::string& name, GLuint glId, unsigned width, unsigned height);
    FGTextureHandle ImportTextureRHI(const std::string& name, rhi::ITexture* texture, unsigned width, unsigned height);

    using ExecuteFunc = std::function<void(const FGPassResources&)>;

    int AddPass(const std::string& name, const ExecuteFunc& execute,
                const std::vector<FGTextureHandle>& reads = {},
                const std::vector<FGTextureHandle>& writes = {},
                bool sideEffect = false);

    void Clear();
    void Compile();
    void Execute();

    struct PassInfo { std::string name; double gpuMs = 0.0; bool culled = false; };
    const std::vector<PassInfo>& GetPasses() const { return m_passInfos; }

    GLuint Resolve(FGTextureHandle h) const;
    rhi::ITexture* ResolveRHI(FGTextureHandle h) const;
    unsigned GetWidth() const { return m_width; }
    unsigned GetHeight() const { return m_height; }
    void SetFrameSize(unsigned w, unsigned h) { m_width = w; m_height = h; }

private:
    struct TextureNode {
        FGTextureDesc desc;
        GLuint glId = 0;
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
        GLuint timeQuery[2] = {0, 0};
        bool culled = false;
    };

    FGTextureHandle AllocHandle();
    void CreateTextureGL(TextureNode& n);

    std::vector<TextureNode> m_textures;
    std::vector<PassNode> m_passes;
    std::vector<PassInfo> m_passInfos;
    unsigned m_width = 0, m_height = 0;
};

} // namespace render
} // namespace eyelib
