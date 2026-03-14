#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/diligent/CommandListDiligent.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceDiligentImpl;

/** Diligent RHI device. When OPENEYE_USE_DILIGENT and engine are linked, can hold real
 *  IRenderDevice/IDeviceContext/ISwapChain; otherwise stub (no-ops). */
struct DeviceDiligent : IDevice {
    /** Stub or real: impl is non-null when real Diligent device was created. */
    DeviceDiligent(void* nwh, uint32_t width, uint32_t height, DeviceDiligentImpl* impl = nullptr);
    ~DeviceDiligent() override;

    /** For CommandListDiligent: immediate context when real, else nullptr. */
    void* GetImmediateContext() const;
    /** For CommandListDiligent: render device when real, else nullptr. */
    void* GetRenderDevice() const;
    /** For CommandListDiligent: swap chain back-buffer RTV when real, else nullptr. */
    void* GetSwapChainBackBufferRTV() const;
    /** For CommandListDiligent: linear sampler for post/copy (ensures blit resources). */
    void* GetLinearSampler();

    const RHICapabilities& GetCapabilities() const override;
    const char* GetShaderBackendName() const override;
    const char* GetShaderVertexExtension() const override;
    const char* GetShaderFragmentExtension() const override;

    ICommandList* CreateCommandList() override;
    void DestroyCommandList(ICommandList* list) override;

    ICommandList* GetImmediateCommandList() override;
    void SubmitCommandList(ICommandList* list) override;

    IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData) override;
    void DestroyBuffer(IBuffer* buffer) override;

    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData) override;
    void DestroyTexture(ITexture* texture) override;
    void CommitSparseTexturePages(ITexture* texture, uint32_t level, int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height, uint32_t depth, bool commit) override;
    void CommitSparseBufferPages(IBuffer* buffer, size_t offset, size_t size, bool commit) override;
    ITexture* CreateTextureView(ITexture* source, RHITextureFormat viewFormat, uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseLayer, uint32_t layerCount) override;
    ITexture* CreateTextureBuffer(IBuffer* buffer, RHITextureFormat format, size_t offset, size_t size) override;
    bool WaitFence(IFence* fence, uint64_t timeoutMs) override;
    void DestroyFence(IFence* fence) override;

    IShader* CreateShader(ShaderStage stage, const char* source, size_t sourceLen) override;
    IShader* CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) override;
    bool TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) override;
    void DestroyShader(IShader* shader) override;

    IPipeline* CreateGraphicsPipeline(const PipelineDesc& desc) override;
    IPipeline* CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology, const DepthStencilState* depthStencilOverride = nullptr) override;
    IPipeline* CreateGraphicsPipelineFromBinary(const void* data, size_t size, uint32_t binaryFormat, const VertexLayout& layout, PrimitiveTopology topology) override;
    bool GetPipelineBinary(IPipeline* pipeline, std::vector<uint8_t>& outData, uint32_t& outFormat) override;
    IPipeline* CreateComputePipeline(void* computeShader) override;
    void DestroyPipeline(IPipeline* pipeline) override;

    IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                      const std::vector<RHITextureFormat>& colorFormats,
                                      RHITextureFormat depthFormat,
                                      uint32_t samples = 1,
                                      bool transient = false) override;
    IRenderTarget* CreateRenderTargetWithSharedDepth(uint32_t width, uint32_t height,
                                                     const std::vector<RHITextureFormat>& colorFormats,
                                                     ITexture* sharedDepthTexture,
                                                     uint32_t samples) override;
    IRenderTarget* CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) override;
    void DestroyRenderTarget(IRenderTarget* rt) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count) override;
    void DestroyQueryPool(IQueryPool* pool) override;

    IPipelineLayout* CreatePipelineLayout(const PipelineLayoutDesc& desc) override;
    void DestroyPipelineLayout(IPipelineLayout* layout) override;
    IDescriptorSet* CreateDescriptorSet(IPipelineLayout* layout) override;
    void DestroyDescriptorSet(IDescriptorSet* set) override;

    IBindlessHeap* CreateBindlessHeap() override;
    void DestroyBindlessHeap(IBindlessHeap* heap) override;

    int GetMaxSamples(int requestedSamples) override;

    ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) override;

    void Present() override;
    void BlitToDefaultFramebuffer(IRenderTarget* src) override;
    void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) override;
    void BlitToDefaultFramebufferSubregion(IRenderTarget* src, int dstX, int dstY, int dstW, int dstH) override;
    void BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) override;
    void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) override;
    void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) override;
    void BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) override;
    bool ReadPixelUint32(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t* outValue) override;
    bool ReadPixelsRGBA8(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t* outBuffer) override;

    void EnsureBlitResources();

    /** For DrawFullscreenQuad: ensure blit/fullscreen quad VB/IB exist; return them. */
    void EnsureFullscreenQuad() { EnsureBlitResources(); }
    IBuffer* GetFullscreenQuadVBO() const { return m_blitVB; }
    IBuffer* GetFullscreenQuadIBO() const { return m_blitIB; }

private:
    RHICapabilities m_caps{};
    std::unique_ptr<CommandListDiligent> m_immediateCommandList;
    std::unique_ptr<DeviceDiligentImpl> m_impl;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    IPipeline* m_blitPipeline = nullptr;
    IBuffer* m_blitVB = nullptr;
    IBuffer* m_blitIB = nullptr;
    void* m_blitSampler = nullptr;  // Diligent::ISampler*
};

} // namespace rhi
} // namespace render
} // namespace eyelib
