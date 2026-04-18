#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace eyelib {
namespace rendering {
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
    ShaderDeviceAddress = 1u << 7,  ///< Buffer may be queried via `IDevice::GetBufferDeviceAddress`. Vulkan: VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT.
    /** Vulkan `VK_EXT_descriptor_buffer`: holds encoded descriptors for bindless / descriptor-buffer bind path. */
    DescriptorBuffer = 1u << 8,
    /** Vulkan `VK_KHR_acceleration_structure`: backing storage for a `VkAccelerationStructureKHR` (`VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR`). */
    AccelerationStructureStorage = 1u << 9,
    /** Vulkan `VK_KHR_acceleration_structure`: scratch memory for `vkCmdBuildAccelerationStructuresKHR` (`VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` is also required; this bit maps to `VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR` when supported). */
    AccelerationStructureBuildInput = 1u << 10,
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

/** `IDevice::SubmitCommandList` (Vulkan): how deferred command lists choose the submit queue when a dedicated compute family exists. */
enum class AsyncComputeSubmitPolicy : uint8_t {
    /** Use `CommandListVK` eligibility + `RHICapabilities::supportsAsyncCompute` + ownership-bridge rules (default). */
    Automatic = 0,
    /** Submit deferred lists on the graphics queue only (debug, deterministic ordering, or “compute after graphics” without async overlap). */
    ForceGraphicsQueue = 1,
};

struct VertexLayout;  // forward
struct IBuffer;       // forward (acceleration-structure backing buffer in RHI)
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
    ///< B8G8R8A8 UNORM; matches typical Windows Vulkan swapchain for transfer copy.
    BGRA8,
    ///< A2B10G10R10 UNORM; common HDR10 swapchain / wide color (Vulkan `VK_FORMAT_A2B10G10R10_UNORM_PACK32`).
    BGR10A2,
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

// ---- Ray tracing / acceleration structures (Vulkan `VK_KHR_acceleration_structure` slice) ----

enum class AccelerationStructureType : uint8_t {
    BottomLevel,
    TopLevel,
};

enum class AccelerationStructureGeometryType : uint8_t {
    Triangles,
    Aabbs,
};

/** Flags for `vkGetAccelerationStructureBuildSizesKHR` / `vkCmdBuildAccelerationStructuresKHR` (subset). */
enum class AccelerationStructureBuildFlags : uint32_t {
    None = 0,
    AllowUpdate = 1u << 0,
    AllowCompaction = 1u << 1,
    PreferFastTrace = 1u << 2,
    PreferFastBuild = 1u << 3,
    LowMemory = 1u << 4,
};
inline AccelerationStructureBuildFlags operator|(AccelerationStructureBuildFlags a, AccelerationStructureBuildFlags b) {
    return static_cast<AccelerationStructureBuildFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline AccelerationStructureBuildFlags& operator|=(AccelerationStructureBuildFlags& a, AccelerationStructureBuildFlags b) {
    a = a | b;
    return a;
}
inline bool HasAccelerationStructureBuildFlag(AccelerationStructureBuildFlags flags, AccelerationStructureBuildFlags bit) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(bit)) != 0;
}

/** One geometry contribution for a bottom-level acceleration structure build-size query. */
struct AccelerationStructureGeometryBuildInput {
    AccelerationStructureGeometryType geometryType = AccelerationStructureGeometryType::Triangles;
    /** Triangles: triangle count; AABBs: AABB primitive count. */
    uint32_t primitiveCount = 0;
    /** Triangles only: max vertex index + 1 (used for sizing). */
    uint32_t maxVertexCount = 0;
};

/** Input for `IDevice::GetAccelerationStructureBuildSizes` (device build, sizing-only). */
struct AccelerationStructureBuildSizesInput {
    AccelerationStructureType type = AccelerationStructureType::BottomLevel;
    AccelerationStructureBuildFlags flags = AccelerationStructureBuildFlags::None;
    /** Bottom-level: one or more geometries. Top-level: leave empty and set `maxInstanceCount`. */
    std::vector<AccelerationStructureGeometryBuildInput> geometries;
    /** Top-level: instance count upper bound for sizing. */
    uint32_t maxInstanceCount = 0;
};

struct AccelerationStructureBuildSizes {
    uint64_t accelerationStructureSize = 0;
    uint64_t buildScratchSize = 0;
    uint64_t updateScratchSize = 0;
};

/**
 * Create a `VkAccelerationStructureKHR` placed in `storageBuffer` at `storageOffset` with `sizeInBytes` from sizing.
 * **Lifetime (Vulkan):** call `IDevice::DestroyAccelerationStructure` **before** `DestroyBuffer` on `storageBuffer` (or any reuse of that memory).
 * The device defers `vkDestroyAccelerationStructureKHR` and buffer frees on the same ring-buffered fence schedule as other resources.
 */
struct AccelerationStructureDesc {
    AccelerationStructureType type = AccelerationStructureType::BottomLevel;
    IBuffer* storageBuffer = nullptr;
    size_t storageOffset = 0;
    uint64_t sizeInBytes = 0;
    const char* debugName = nullptr;
};

/** Byte stride of one Vulkan `VkAccelerationStructureInstanceKHR` (see `AccelerationStructureBuffersVK.cpp` static_assert). */
constexpr size_t kAccelerationStructureInstanceRecordStrideBytes = 64;

/** Scratch memory for `vkCmdBuildAccelerationStructuresKHR` (`IDevice::CreateAccelerationStructureScratchBuffer`). */
struct AccelerationStructureScratchBufferDesc {
    /** Minimum `GetAccelerationStructureBuildSizes::buildScratchSize` from the matching build; device aligns to AS scratch rules. */
    size_t sizeInBytes = 0;
    BufferMemoryHint memoryHint = BufferMemoryHint::DeviceOnly;
    const char* debugName = nullptr;
};

/** TLAS instance array (`VkAccelerationStructureInstanceKHR` records). */
struct AccelerationStructureInstanceBufferDesc {
    uint32_t maxInstanceCount = 0;
    /** Bytes between consecutive records; **0** = `kAccelerationStructureInstanceRecordStrideBytes`. */
    size_t strideInBytes = 0;
    BufferMemoryHint memoryHint = BufferMemoryHint::DeviceOnly;
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
    /** Scratch buffer for `vkCmdBuildAccelerationStructuresKHR` (Vulkan: `VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR` + shader storage access on buffer). */
    AsBuildScratch,
    /** Read-only geometry/instance inputs to an AS build (Vulkan: AS BUILD stage + `VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR` on buffer). */
    AsBuildGeometryInput,
    /** BLAS/TLAS backing storage being written by an AS build (Vulkan: AS BUILD + `VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR`). */
    AsBuildStorage,
};

// ---- Shader ----

enum class ShaderStage {
    Vertex,
    Fragment,
    Compute,
    TessControl,
    TessEvaluation,
    Geometry,
    /** Vulkan `VK_EXT_mesh_shader` task payload stage (optional). */
    Task,
    /** Vulkan `VK_EXT_mesh_shader` mesh output stage (replaces vertex shader for that pipeline). */
    Mesh,
};

/** Vulkan `VK_KHR_fragment_shading_rate`: per-draw fragment shading rate (pixel footprint). */
enum class FragmentShadingRate : uint8_t {
    Rate1x1 = 0,
    Rate1x2,
    Rate2x1,
    Rate2x2,
    Rate2x4,
    Rate4x2,
    Rate4x4,
};

/** Vulkan `VkFragmentShadingRateCombinerOpKHR` mapped to backend-agnostic command intent. */
enum class FragmentShadingRateCombinerOp : uint8_t {
    Keep = 0,
    Replace,
    Min,
    Max,
    Mul,
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
    ComputeLightCull,     ///< set0 Frame, set1 LightCullPass, set2-3 Empty placeholders, set4 SceneBuffers
    ComputeGpuCull,       ///< set0 Frame, set1-3 Empty placeholders, set4 SceneBuffers
    IBL,                  ///< set0 IBLPass (binding 0 = environment cube); push: view, projection [, roughness]
    OutlinePost,          ///< set0 OutlinePost (binding 0 = scene color, 1 = entity ID); push: selectedID, outlineColor, invSize
    /** set0 Frame only (per-frame + per-view UBOs); push constants unchanged. EyelibRendering bootstrap / simple passes. */
    ComposeBootstrap,
};

/** Vulkan: how to obtain SPIR-V for a shader asset. OpenGL/Diligent ignore where not applicable. */
enum class ShaderSourcePolicy : uint32_t {
    PreferSpirv = 0,      ///< Try precompiled .spv on disk first; if missing, compile GLSL from disk or overrides.
    SpirvOnly,            ///< Only load precompiled SPIR-V (fails if missing).
    CompileFromSource,    ///< Compile GLSL from disk or from non-empty vertex/fragment source overrides.
};

/** Color + depth/stencil formats for dynamic rendering (Vulkan VkPipelineRenderingCreateInfo). Empty color + depth None = depth-only pass (e.g. shadow map). */
struct PipelineAttachmentFormats {
    std::vector<RHITextureFormat> color;
    RHITextureFormat depthStencil = RHITextureFormat::None;
};

/** Create a graphics pipeline from one logical shader asset (path without extension, e.g. passes/sky/skybox). */
struct ShaderGraphicsPipelineCreateInfo {
    /** Logical path under assets/shaders/{backend}/; may be empty only when both source overrides are set. */
    std::string assetPath;
    VertexLayout vertexLayout;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    PipelineLayoutKind layoutKind = PipelineLayoutKind::PushOnly;
    bool useDepthStencilOverride = false;
    DepthStencilState depthStencilOverride{};
    ShaderSourcePolicy sourcePolicy = ShaderSourcePolicy::PreferSpirv;
    /** Prepended as #define KEY 1 when loading/compiling GLSL (not applied to SPIR-V-only path). */
    std::vector<std::string> keywordDefines;
    /** When both overrides are non-empty, GLSL is taken from here instead of disk (e.g. MaterialCompiler). */
    std::string vertexSourceOverride;
    std::string fragmentSourceOverride;
    /** When set, Vulkan uses these for VkPipelineRenderingCreateInfo (must match BeginRendering attachments). When unset, formats are inferred from layoutKind only. */
    std::optional<PipelineAttachmentFormats> attachmentFormats;
};

struct PipelineDesc {
    void* vertexShader = nullptr;    // IShader* (typed in RHI.h)
    void* fragmentShader = nullptr;
    void* computeShader = nullptr;
    void* tessControlShader = nullptr;   // optional; requires TessEvaluation if set
    void* tessEvalShader = nullptr;     // optional
    void* geometryShader = nullptr;     // optional
    /** Vulkan `VK_EXT_mesh_shader`: optional task shader (`ShaderStage::Task`). Requires `meshShader` and no `vertexShader`. */
    void* taskShader = nullptr;
    /** When set with `fragmentShader` and **without** `vertexShader`, creates a mesh graphics pipeline (`ShaderStage::Mesh`). */
    void* meshShader = nullptr;
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
    /** Vulkan dynamic rendering: when set, must match **`vkCmdBeginRendering`** color/depth views for passes that use this pipeline (see **`dynamic_rendering_pass_contract.md`** in `Vulkan/`). When unset, inferred from **`layoutKind`** (Post should usually be set explicitly). */
    std::optional<PipelineAttachmentFormats> attachmentFormats;
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

/** Bit mask compatible with `VkQueryPipelineStatisticFlagBits` (core bits only). */
using PipelineStatisticsFlags = uint32_t;

/** Default pipeline-statistics counters when `pipelineStatisticsMask == 0` in `IDevice::CreateQueryPool`: IA vertices, IA primitives, VS invocations, FS invocations. */
inline constexpr PipelineStatisticsFlags kDefaultPipelineStatisticsFlags =
    (1u << 0) | (1u << 1) | (1u << 2) | (1u << 7); // IA_VERTICES, IA_PRIMITIVES, VS_INVOCATIONS, FS_INVOCATIONS

enum class QueryType {
    Timestamp,
    Occlusion, ///< Samples that pass depth/stencil tests; pair with `ICommandList::ResetQueryPool` before use.
    /** `vkCmdBeginQuery`/`vkCmdEndQuery` with `VK_QUERY_TYPE_PIPELINE_STATISTICS`; read via `IQueryPool::GetPipelineStatistics`. */
    PipelineStatistics,
};

/** Request for Vulkan swapchain `VkPresentModeKHR`; backend picks a supported mode (FIFO is always the safe fallback). */
enum class SwapchainPresentMode {
    Fifo,      ///< V-sync (FIFO); typically always available.
    Mailbox,   ///< Prefer low-latency triple-buffer when supported.
    Immediate, ///< Prefer lowest latency; may tear when supported.
};

/** Vulkan swapchain surface format / color space preference (requires `VK_EXT_swapchain_colorspace` + driver support for HDR modes). */
enum class SwapchainOutputMode : uint8_t {
    /** 8-bit sRGB non-linear (e.g. `B8G8R8A8_UNORM` + `SRGB_NONLINEAR`). */
    SdrSrgb,
    /** HDR10 / ST2084 PQ (e.g. `A2B10G10R10` + `HDR10_ST2084_EXT`). */
    Hdr10St2084,
    /** scRGB-style extended linear (e.g. `R16G16B16A16_SFLOAT` + `EXTENDED_SRGB_LINEAR_EXT`). */
    HdrExtendedSrgbLinear,
};

/** Maps a nominal frame cap (Hz) to nanoseconds for `IDevice::WaitForPresent` / `WaitForPreviousPresent` timeouts. `0` → `UINT64_MAX` (no timeout budget). Frame-loop recipe: `EngineSource/eyelib/rendering/rhi/Vulkan/swapchain_present_policy.md` §4. */
inline uint64_t PresentWaitTimeoutNsFromTargetHz(uint32_t targetHz) {
    if (targetHz == 0) return UINT64_MAX;
    return 1000000000ull / static_cast<uint64_t>(targetHz);
}

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
    /** GPU-driven GBuffer indirect path with per-draw model matrix fetch in VS (DrawID/BaseInstance + SSBO transform buffer). */
    bool supportsGpuDrivenPerDrawTransformIndirect = false;
    /** When true, GetResourceSetLayout returns valid layouts and BindDescriptorSets applies correctly.
     *  Vulkan: true. OpenGL: false until shaders use descriptor_binding_table layout(binding=N). */
    bool supportsResourceSpaceBindPath = false;
    /** Vulkan: surface lists `VK_PRESENT_MODE_MAILBOX_KHR` (low-latency / triple-buffer style when used). */
    bool supportsPresentMailbox = false;
    /** Vulkan: surface lists `VK_PRESENT_MODE_IMMEDIATE_KHR` (may tear). */
    bool supportsPresentImmediate = false;
    /** Vulkan: `VkPhysicalDeviceLimits::timestampPeriod` > 0 (GPU timestamp queries usable). */
    bool supportsGpuTimestampQueries = false;
    /** Vulkan: `VkPhysicalDeviceFeatures::occlusionQuery` (occlusion query pools). */
    bool supportsOcclusionQueries = false;
    /** Vulkan: `VkPhysicalDeviceHostQueryResetFeatures::hostQueryReset` — `vkResetQueryPool` on host (`IDevice::ResetQueryPoolOnHost`). */
    bool supportsHostQueryReset = false;
    /** Vulkan: `VkPhysicalDeviceFeatures::pipelineStatisticsQuery` (pipeline statistics query pools). */
    bool supportsPipelineStatisticsQueries = false;
    /** Vulkan: `VkPhysicalDeviceSynchronization2Features::synchronization2` (`vkCmdPipelineBarrier2` path). */
    bool supportsSynchronization2 = false;
    /** Vulkan: `VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress` (`vkGetBufferDeviceAddress`). */
    bool supportsBufferDeviceAddress = false;
    /** Vulkan: optional `VK_EXT_swapchain_maintenance1` (device + swapchain present-mode chain when supported). */
    bool supportsSwapchainMaintenance1 = false;
    /** Vulkan: `VK_EXT_swapchain_colorspace` enabled (additional `VkSurfaceFormatKHR` pairs for wide gamut / HDR). */
    bool supportsSwapchainColorspace = false;
    /** Vulkan: `VK_KHR_present_id` + `VK_KHR_present_wait` (`vkWaitForPresentKHR` for present completion). */
    bool supportsPresentWait = false;
    /** Vulkan: `VK_EXT_descriptor_buffer` feature enabled (descriptor-buffer API path available). */
    bool supportsDescriptorBuffer = false;
    /** Vulkan: `VK_EXT_graphics_pipeline_library` feature enabled (monolithic create may chain `VkGraphicsPipelineLibraryCreateInfoEXT`). */
    bool supportsGraphicsPipelineLibrary = false;
    /** Vulkan: `VK_KHR_pipeline_executable_properties` enabled; entry point loaded when advertised. After the first successful `vkCreate*Pipelines`, **`DeviceVK`** may emit one **Info** sample line (executable count + first ISA name). */
    bool supportsPipelineExecutableProperties = false;
    /** Vulkan: `VK_EXT_device_fault` enabled (driver may report fault info; first slice is cap + optional device create + log-only). */
    bool supportsDeviceFault = false;
    /** Vulkan: `VK_EXT_mesh_shader` enabled (`vkCmdDrawMeshTasksEXT` path). */
    bool supportsMeshShader = false;
    /** Vulkan: mesh shader task stage enabled (`taskShader` pipelines). */
    bool supportsMeshTaskShader = false;
    /** From `VkPhysicalDeviceMeshShaderPropertiesEXT::maxMeshOutputVertices` when supported; else 0. */
    uint32_t meshShaderMaxOutputVertices = 0;
    /** From `maxMeshOutputPrimitives`. */
    uint32_t meshShaderMaxOutputPrimitives = 0;
    /** From `maxMeshPayloadAndSharedMemorySize` (bytes). */
    uint32_t meshShaderMaxPayloadAndSharedMemoryBytes = 0;
    /** Vulkan: `VK_KHR_fragment_shading_rate` dynamic state path (`vkCmdSetFragmentShadingRateKHR`). */
    bool supportsVariableRateShading = false;
    /** Vulkan: min texel size for shading-rate attachment / pipeline rate (`VkPhysicalDeviceFragmentShadingRatePropertiesKHR`). */
    uint32_t vrsMinTexelWidth = 1;
    uint32_t vrsMinTexelHeight = 1;
    /** Vulkan: max texel size for shading-rate attachment / pipeline rate (`VkPhysicalDeviceFragmentShadingRatePropertiesKHR`). */
    uint32_t vrsMaxTexelWidth = 1;
    uint32_t vrsMaxTexelHeight = 1;
    /** Vulkan ray tracing slice support (Phase 11): true when acceleration structures + ray query are enabled. */
    bool supportsRayQuery = false;
    /** True when `vkCmdBuildAccelerationStructuresKHR` is loaded (same enable chain as `supportsRayQuery` today). Distinct name for policy/gating AS **build** commands vs future RTPSO-only caps when Phase 11 completes. */
    bool supportsAccelerationStructureBuild = false;
    /** Vulkan AS property: `maxPrimitiveCount` (upper bound for one geometry build call). */
    uint64_t rayTracingMaxPrimitiveCount = 0;
    /** Vulkan AS property: `minAccelerationStructureScratchOffsetAlignment`. */
    uint32_t rayTracingScratchAlignment = 0;
    /** From `VkPhysicalDeviceSubgroupProperties::subgroupSize` (`vkGetPhysicalDeviceProperties2`); 0 if not queried. */
    uint32_t subgroupSize = 0;
    /** `VkPhysicalDeviceSubgroupProperties::supportedStages` as `uint32_t` (`VkShaderStageFlags`). */
    uint32_t subgroupSupportedShaderStagesMask = 0;
    /** `VkPhysicalDeviceSubgroupProperties::supportedOperations` as `uint32_t` (`VkSubgroupFeatureFlags`). */
    uint32_t subgroupSupportedOperationsMask = 0;
    /** `VkPhysicalDeviceSubgroupProperties::quadOperationsInAllStages`. */
    bool subgroupQuadOperationsInAllStages = false;
};

} // namespace rhi
} // namespace rendering
} // namespace eyelib
