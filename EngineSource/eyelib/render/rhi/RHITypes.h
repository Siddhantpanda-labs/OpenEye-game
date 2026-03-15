#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

// ---- Buffer ----

enum class BufferUsage : uint32_t {
    Vertex      = 1u << 0,
    Index       = 1u << 1,
    Uniform     = 1u << 2,
    Storage     = 1u << 3,
    TransferSrc = 1u << 4,
    TransferDst = 1u << 5,
    IndirectDraw = 1u << 6,  ///< Buffer holds DrawIndirectCommand(s); required for Draw*Indirect. Vulkan: VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT.
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
    DeviceOnly,            // GPU only
    HostVisible,          // CPU visible (upload or readback)
    HostVisiblePersistent, // CPU visible, persistent map (ARB_buffer_storage; high-throughput streaming)
};

struct VertexLayout;  // forward
struct BufferDesc {
    size_t sizeInBytes = 0;
    BufferUsage usage = BufferUsage::Vertex;
    BufferMemoryHint memoryHint = BufferMemoryHint::DeviceOnly;
    const VertexLayout* vertexLayout = nullptr;  // optional; set when creating mesh VBs for full layout
    /** If true and backend supports sparse buffer (ARB_sparse_buffer), create as sparse; commit pages via CommitSparseBufferPages. */
    bool sparse = false;
    /** Optional GPU debug name (e.g. Vulkan: vkSetDebugUtilsObjectNameEXT). */
    const char* debugName = nullptr;
};

// ---- Texture ----

enum class TextureType {
    Texture2D,
    Texture2DArray,
    TextureCube,
    Texture3D,
    TextureBuffer,  ///< Buffer-backed texture (TBO); created via CreateTextureBuffer
};

enum class RHITextureFormat {
    R8,
    RG8,
    RGB8,
    RGBA8,
    RGB8_SRGB,   ///< sRGB decode on sample (for LDR images: JPG/PNG skybox, etc.)
    RGBA8_SRGB,
    RGB16F,
    RGBA16F,
    R32UI,  ///< Single-channel unsigned int (e.g. entity ID buffer)
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
    /** If true and backend supports sparse (ARB_sparse_texture), create as sparse/virtual. */
    bool sparse = false;
    /** If true, backend may use transient attachment (e.g. Vulkan VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT + lazily allocated memory). Attachment-only; not for sampling. */
    bool transient = false;
    /** Optional GPU debug name (e.g. Vulkan: vkSetDebugUtilsObjectNameEXT for image and image view). */
    const char* debugName = nullptr;
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
    /** Buffer read by GPU as indirect draw/dispatch parameters. Vulkan: VK_ACCESS_INDIRECT_COMMAND_READ_BIT. */
    IndirectDraw,
};

// ---- Shader ----

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
    TessControl,
    TessEvaluation,
    Geometry,
};

// ---- Pipeline ----

enum class PrimitiveTopology {
    TriangleList,
    TriangleStrip,
    PointList,
    LineList,
    PatchList,  ///< For tessellation; patch vertex count set in PipelineDesc::patchVertexCount
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

enum class DepthCompare {
    Less,      // pass when fragment depth < buffer (default for opaque)
    LessEqual, // pass when fragment depth <= buffer (e.g. skybox at depth 1.0)
    Greater,
};

struct DepthStencilState {
    bool depthTest = true;
    bool depthWrite = true;
    DepthCompare depthCompare = DepthCompare::Less;
    bool stencilTest = false;
};

struct RasterState {
    bool cullEnable = false;
    bool frontCounterClockwise = false;  // GL: CCW front
    bool depthBiasEnable = false;
    float depthBiasConstant = 0.f;
    float depthBiasSlope = 0.f;
};

/** Pipeline layout kind: which descriptor sets a pipeline uses. Vulkan: resource-space only (Legacy treated as PushOnly). */
enum class PipelineLayoutKind : uint32_t {
    PushOnly = 0,         ///< No descriptor sets; push constants only (e.g. BRDF LUT, fullscreen)
    Legacy = 0,           ///< Alias for PushOnly (no legacy descriptor path)
    Forward,              ///< set0 Frame, set1 ForwardPass, set2 Material, set3 Bindless, set4 SceneBuffers
    Post,                 ///< set0 Frame, set1 PostPass
    Shadow,               ///< set0 Frame, set1 ShadowPass, set2 SceneBuffers
    GBuffer,              ///< set0 Frame, set1 Material, set2 Bindless, set3 SceneBuffers
    DeferredLighting,     ///< set0 Frame, set1 DeferredLightingPass
    ComputeLightCull,     ///< set0 Frame, set1 LightCullPass, set2 SceneBuffers
    ComputeGpuCull,       ///< set0 Frame, set1 SceneBuffers
    IBL,                  ///< set0 IBLPass (binding 0 = environment cube); push: view, projection [, roughness]
    OutlinePost,          ///< set0 OutlinePost (binding 0 = scene color, 1 = entity ID); push: selectedID, outlineColor, invSize
};

struct PipelineDesc {
    void* vertexShader = nullptr;    // IShader* (typed in RHI.h)
    void* fragmentShader = nullptr;
    void* computeShader = nullptr;
    void* tessControlShader = nullptr;   // optional; requires TessEvaluation if set
    void* tessEvalShader = nullptr;     // optional
    void* geometryShader = nullptr;     // optional
    VertexLayout vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    /** Patch size when using tessellation (PatchList or tess shaders present). Default 3 = triangle patches. */
    uint32_t patchVertexCount = 3;
    /** Transform feedback: varyings to capture (names as in shader). Set before CreateGraphicsPipeline. */
    std::vector<const char*> transformFeedbackVaryings;
    BlendState blend;
    DepthStencilState depthStencil;
    RasterState raster;
    /** Optional GPU debug name (e.g. Vulkan: vkSetDebugUtilsObjectNameEXT). */
    const char* debugName = nullptr;
    /** Which descriptor set layout to use. Legacy = current path; other kinds use resource-space layouts (set0 Frame, set1 Pass, etc.). Default Legacy. */
    PipelineLayoutKind layoutKind = PipelineLayoutKind::PushOnly;
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
    /** When clearing an integer color RT (e.g. R32UI), use this instead of color[]. */
    uint32_t colorUint = 0;
    float depth = 1.f;
    uint32_t stencil = 0;
};

// ---- Pipeline layout & descriptors (Phase 3: Vulkan prep) ----

/** Type of a single binding in a descriptor set / pipeline layout. */
enum class BindingType : uint32_t {
    Texture,
    TextureCube,
    UniformBuffer,
    StorageBuffer,
};

/** Describes one binding (slot) in a pipeline layout. */
struct BindingDesc {
    uint32_t binding = 0;
    BindingType type = BindingType::Texture;
};

/** Describes the full layout of one descriptor set (set index 0 for now). Vulkan: maps to VkDescriptorSetLayout. */
struct PipelineLayoutDesc {
    std::vector<BindingDesc> bindings;
};

// ---- Indirect draw (Phase 4: GPU-driven) ----

/** Layout matches GL DrawArraysIndirectCommand / Vulkan VkDrawIndirectCommand. */
struct DrawArraysIndirectCommand {
    uint32_t vertexCount = 0;
    uint32_t instanceCount = 0;
    uint32_t firstVertex = 0;
    uint32_t baseInstance = 0;
};

/** Layout matches GL DrawElementsIndirectCommand / Vulkan VkDrawIndexedIndirectCommand. */
struct DrawIndexedIndirectCommand {
    uint32_t indexCount = 0;
    uint32_t instanceCount = 0;
    uint32_t firstIndex = 0;
    int32_t  vertexOffset = 0;
    uint32_t baseInstance = 0;
};

/** Layout matches GL DispatchIndirectCommand / Vulkan VkDispatchIndirectCommand. Used for DispatchIndirect. */
struct DispatchIndirectCommand {
    uint32_t numGroupsX = 0;
    uint32_t numGroupsY = 0;
    uint32_t numGroupsZ = 0;
};

// ---- Query ----

enum class QueryType {
    Timestamp,
};

// ---- Bindless (Phase 4.4 skeleton) ----

/** Opaque handle for a bindless texture or buffer. 0 = invalid. Backend-specific (e.g. GL uint64, Vulkan descriptor index). */
using BindlessHandle = uint64_t;
constexpr BindlessHandle kInvalidBindlessHandle = 0;

// ---- Capabilities ----

/** Backend capabilities; query via IDevice::GetCapabilities(). Branch on caps, not on backend type. */
struct RHICapabilities {
    bool supportsBindless = false;           ///< Bindless textures/buffers (e.g. ARB_bindless_texture, descriptor indexing)
    bool supportsCompute = false;            ///< Compute shaders / Dispatch
    bool supportsMultiDrawIndirect = false;  ///< Multi-draw indirect (glMultiDrawArraysIndirect etc.)
    bool supportsMultiDrawIndirectCount = false;  ///< Multi-draw indirect with count from buffer (GL_ARB_indirect_parameters; GPU-written draw count)
    bool supportsAsyncCompute = false;      ///< Async compute queue (Vulkan-style)
    bool supportsRayTracing = false;         ///< Ray tracing (optional; future)
    bool supportsTessellation = false;       ///< Tessellation control/eval shaders (OpenGL 4.0+)
    bool supportsGeometryShader = false;    ///< Geometry shader stage
    bool supportsTransformFeedback = false; ///< Transform feedback (capture vertex outputs to buffer)
    bool supportsConditionalRender = false; ///< Conditional render (skip draw based on query result)
    bool supportsSparseTexture = false;     ///< Sparse/virtual textures (ARB_sparse_texture)
    bool supportsSparseBuffer = false;      ///< Sparse buffers (ARB_sparse_buffer)
    bool supportsAtomicCounters = false;    ///< Atomic counter buffers (ARB_shader_atomic_counters)
    bool supportsShaderSubroutines = false; ///< Shader subroutines (ARB_shader_subroutine)
};

} // namespace rhi
} // namespace render
} // namespace eyelib
