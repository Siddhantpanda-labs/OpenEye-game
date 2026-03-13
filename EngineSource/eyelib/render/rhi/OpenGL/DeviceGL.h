#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/OpenGL/CommandListGL.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceGL : IDevice {
    DeviceGL();
    ~DeviceGL() override;

    const RHICapabilities& GetCapabilities() const override;
    const char* GetShaderBackendName() const override;
    const char* GetShaderVertexExtension() const override;
    const char* GetShaderFragmentExtension() const override;
    bool ResolveShaderPath(const char* logicalName, ShaderStage stage, std::string& outPath) override;

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
    void EnsureComputePipelineStorageBlockBindings(IPipeline* pipeline) override;
    bool ComputePipelineHasStorageBlock(IPipeline* pipeline, const char* blockName) override;

    IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                      const std::vector<RHITextureFormat>& colorFormats,
                                      RHITextureFormat depthFormat,
                                      uint32_t samples) override;
    IRenderTarget* CreateDepthOnlyRenderTargetArray(uint32_t width, uint32_t height, uint32_t arrayLayers) override;
    IRenderTarget* CreateRenderTargetWithSharedDepth(uint32_t width, uint32_t height,
                                                      const std::vector<RHITextureFormat>& colorFormats,
                                                      ITexture* sharedDepthTexture,
                                                      uint32_t samples) override;
    IRenderTarget* CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) override;
    IRenderTarget* CreateRenderTargetFromTextureWithDepth(ITexture* colorTexture, uint32_t face, uint32_t mipLevel, ITexture* depthTexture) override;
    void DestroyRenderTarget(IRenderTarget* rt) override;

    ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count) override;
    void DestroyQueryPool(IQueryPool* pool) override;

    IPipelineLayout* CreatePipelineLayout(const PipelineLayoutDesc& desc) override;
    void DestroyPipelineLayout(IPipelineLayout* layout) override;
    IDescriptorSet* CreateDescriptorSet(IPipelineLayout* layout) override;
    void DestroyDescriptorSet(IDescriptorSet* set) override;

    IBindlessHeap* CreateBindlessHeap() override;
    void DestroyBindlessHeap(IBindlessHeap* heap) override;

    int GetMaxSamples(int requestedSamples) override;

    void Present() override;
    void Finish() override;
    void BlitToDefaultFramebuffer(IRenderTarget* src) override;
    void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) override;
    void BlitToDefaultFramebufferSubregion(IRenderTarget* src, int dstX, int dstY, int dstW, int dstH) override;
    void BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) override;
    void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) override;
    void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) override;
    void BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) override;
    bool ReadPixelUint32(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t* outValue) override;
    bool ReadPixelsRGBA8(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t* outBuffer) override;
    void* CreatePixelPackBuffer(size_t sizeInBytes) override;
    void DestroyPixelPackBuffer(void* handle) override;
    void ReadPixelsToPBO(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, void* pboHandle) override;
    void MapPixelPackBuffer(void* pboHandle, void* outBuffer, size_t sizeInBytes, uint32_t width, uint32_t height) override;
    void MapPixelPackBufferRaw(void* pboHandle, void* outBuffer, size_t sizeInBytes) override;

    /** Lazy-create and return the shared fullscreen quad VBO/IBO used by DrawFullscreenQuad. */
    void EnsureFullscreenQuad();
    IBuffer* GetFullscreenQuadVBO() const { return m_fullscreenQuadVBO; }
    IBuffer* GetFullscreenQuadIBO() const { return m_fullscreenQuadIBO; }

private:
    void QueryCapabilities();
    RHICapabilities m_caps;
    std::unique_ptr<CommandListGL> m_immediateCommandList;
    IBuffer* m_fullscreenQuadVBO = nullptr;
    IBuffer* m_fullscreenQuadIBO = nullptr;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
