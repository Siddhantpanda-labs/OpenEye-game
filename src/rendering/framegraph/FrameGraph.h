#pragma once

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <glad/glad.h>

namespace OpenEye { namespace Rendering { class Texture; } }

namespace OpenEye { namespace Rendering {

struct FGTextureDesc {
    unsigned width = 0;
    unsigned height = 0;
    GLenum internalFormat = GL_SRGB8_ALPHA8;
    bool srgb = true;
    std::string name;
};

using FGTextureHandle = int; // -1 invalid

class FrameGraph;

struct FGPassResources {
    // Map from logical handles to GL texture ids
    std::function<GLuint(FGTextureHandle)> getTexture;
    unsigned width = 0;
    unsigned height = 0;
};

class FrameGraph {
public:
    FrameGraph();
    ~FrameGraph();

    // Resource API
    FGTextureHandle createTexture(const FGTextureDesc& desc);
    FGTextureHandle importTexture(const std::string& name, GLuint glId, unsigned width, unsigned height);

    // Pass API
    using SetupFunc = std::function<void(FrameGraph& fg)>; // for future resource wiring
    using ExecuteFunc = std::function<void(const FGPassResources&)>;

    int addPass(const std::string& name, const ExecuteFunc& execute,
                const std::vector<FGTextureHandle>& reads = {},
                const std::vector<FGTextureHandle>& writes = {},
                bool sideEffect = false);

    void clear();
    void compile(); // build execution order
    void execute(); // run passes with GPU timings

    // Introspection
    struct PassInfo { std::string name; double gpuMs = 0.0; bool culled = false; };
    const std::vector<PassInfo>& getPasses() const { return m_passInfos; }

    // Resolve handle to GL texture id
    GLuint resolve(FGTextureHandle h) const;
    unsigned getWidth() const { return m_width; }
    unsigned getHeight() const { return m_height; }
    void setFrameSize(unsigned w, unsigned h) { m_width = w; m_height = h; }

private:
    struct TextureNode {
        FGTextureDesc desc;
        GLuint glId = 0; // 0 until imported/created
        bool imported = false;
        unsigned width = 0;
        unsigned height = 0;
    };

    struct PassNode {
        std::string name;
        ExecuteFunc execute;
        std::vector<FGTextureHandle> reads;
        std::vector<FGTextureHandle> writes;
        bool sideEffect = false;
        GLuint timeQuery[2] = {0, 0};
        bool culled = false;
    };

    FGTextureHandle allocHandle();

    std::vector<TextureNode> m_textures; // index == FGTextureHandle
    std::vector<PassNode> m_passes;
    std::vector<PassInfo> m_passInfos;
    unsigned m_width = 0, m_height = 0;
};

} }


