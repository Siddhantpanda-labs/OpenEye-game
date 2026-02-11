#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

// ---- Buffer ----

enum class BufferUsage : uint32_t {
    Vertex     = 1u << 0,
    Index      = 1u << 1,
    Uniform    = 1u << 2,
    Storage    = 1u << 3,
    TransferSrc = 1u << 4,
    TransferDst = 1u << 5,
};
inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline BufferUsage& operator|=(BufferUsage& a, BufferUsage b) { a = a | b; return a; }
inline bool HasUsage(BufferUsage flags, BufferUsage bit) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(bit)) != 0;
}

/** CPU can read/write (e.g. for upload or readback). */
enum class BufferMemoryHint {
    DeviceOnly,   // GPU only
    HostVisible,  // CPU visible (upload or readback)
};

struct VertexLayout;  // forward
struct BufferDesc {
    size_t sizeInBytes = 0;
    BufferUsage usage = BufferUsage::Vertex;
    BufferMemoryHint memoryHint = BufferMemoryHint::DeviceOnly;
    const VertexLayout* vertexLayout = nullptr;  // optional; set when creating mesh VBs so BGFX can use full layout
};

// ---- Texture ----

enum class TextureType {
    Texture2D,
    Texture2DArray,
    TextureCube,
    Texture3D,
};

enum class RHITextureFormat {
    R8,
    RG8,
    RGB8,
    RGBA8,
    RGB16F,
    RGBA16F,
    Depth24,
    Depth24Stencil8,
    None,
};

enum class TextureUsage : uint32_t {
    ShaderResource = 1u << 0,
    RenderTarget   = 1u << 1,
    TransferSrc    = 1u << 2,
    TransferDst    = 1u << 3,
};
inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool HasUsage(TextureUsage flags, TextureUsage bit) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(bit)) != 0;
}

struct TextureDesc {
    TextureType type = TextureType::Texture2D;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depthOrArrayLayers = 1;  // depth for 3D, array size for 2DArray
    RHITextureFormat format = RHITextureFormat::RGBA8;
    TextureUsage usage = TextureUsage::ShaderResource;
    uint32_t mipLevels = 1;
    uint32_t samples = 1;  // 1 = no MSAA
};

// ---- Resource states (for barriers) ----

enum class ResourceState : uint32_t {
    Undefined,
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    ShaderResource,
    RenderTarget,
    DepthWrite,
    DepthRead,
    TransferDst,
    TransferSrc,
    Present,
};

// ---- Shader ----

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
};

// ---- Pipeline ----

enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    PointList,
    LineList,
};

enum class IndexType {
    UInt16,
    UInt32,
};

enum class VertexAttributeType {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    UInt4,
};

struct VertexAttribute {
    uint32_t location = 0;
    VertexAttributeType type = VertexAttributeType::Float3;
    uint32_t offsetInBytes = 0;
};

struct VertexLayout {
    uint32_t strideInBytes = 0;
    std::vector<VertexAttribute> attributes;
};

enum class BlendFactor {
    Zero, One, SrcAlpha, OneMinusSrcAlpha, DstAlpha, OneMinusDstAlpha,
    SrcColor, OneMinusSrcColor, DstColor, OneMinusDstColor,
};

enum class BlendOp {
    Add, Subtract, Min, Max,
};

struct BlendState {
    bool blendEnable = false;
    BlendFactor srcColor = BlendFactor::SrcAlpha;
    BlendFactor dstColor = BlendFactor::OneMinusSrcAlpha;
    BlendOp colorOp = BlendOp::Add;
    BlendFactor srcAlpha = BlendFactor::One;
    BlendFactor dstAlpha = BlendFactor::Zero;
    BlendOp alphaOp = BlendOp::Add;
};

struct DepthStencilState {
    bool depthTest = true;
    bool depthWrite = true;
    bool depthCompareLess = true;  // true = less, false = greater
    bool stencilTest = false;
};

struct RasterState {
    bool cullEnable = false;
    bool frontCounterClockwise = false;  // GL: CCW front
    bool depthBiasEnable = false;
    float depthBiasConstant = 0.f;
    float depthBiasSlope = 0.f;
};

struct PipelineDesc {
    void* vertexShader = nullptr;    // IShader* (typed in RHI.h)
    void* fragmentShader = nullptr;
    void* computeShader = nullptr;
    VertexLayout vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    BlendState blend;
    DepthStencilState depthStencil;
    RasterState raster;
};

// ---- Render target ----

enum class LoadAction {
    Load,   // keep existing
    Clear,  // clear before use
};

enum class StoreAction {
    Store,  // keep after pass
    DontCare,
};

struct ClearValue {
    float color[4] = {0.f, 0.f, 0.f, 1.f};
    float depth = 1.f;
    uint32_t stencil = 0;
};

// ---- Query ----

enum class QueryType {
    Timestamp,
};

} // namespace rhi
} // namespace render
} // namespace eyelib
