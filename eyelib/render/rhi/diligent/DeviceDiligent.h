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
    /** For CommandListDiligent: swap chain back-buffer RTV when real, else nullptr. */
    void* GetSwapChainBackBufferRTV() const;
    /** For CommandListDiligent: Diligent IRenderDevice* when real, else nullptr. */
    void* GetDiligentDevice() const;

    ICommandList* GetImmediateCommandList() override;
    void SubmitCommandList(ICommandList* list) override;

    IBuffer* CreateBuffer(const BufferDesc& desc, const void* initialData) override;
    void DestroyBuffer(IBuffer* buffer) override;

    ITexture* CreateTexture(const TextureDesc& desc, const void* initialData) override;
    void DestroyTexture(ITexture* texture) override;

    IShader* CreateShader(ShaderStage stage, const char* source, size_t sourceLen) override;
    IShader* CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) override;
    bool TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) override;
    void DestroyShader(IShader* shader) override;

    IPipeline* CreateGraphicsPipeline(const PipelineDesc& desc) override;
    IPipeline* CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology) override;
    IPipeline* CreateComputePipeline(void* computeShader) override;
    void DestroyPipeline(IPipeline* pipeline) override;

    IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                      const std::vector<RHITextureFormat>& colorFormats,
                                      RHITextureFormat depthFormat,
                                      uint32_t samples) override;
    IRenderTarget* CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) override;
    void DestroyRenderTarget(IRenderTarget* rt) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count) override;
    void DestroyQueryPool(IQueryPool* pool) override;

    int GetMaxSamples(int requestedSamples) override;

    ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) override;

    void Present() override;
    void BlitToDefaultFramebuffer(IRenderTarget* src) override;
    void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) override;
    void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) override;
    void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) override;
    void BlitDepthOnlyFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst) override;

private:
    std::unique_ptr<CommandListDiligent> m_immediateCommandList;
    std::unique_ptr<DeviceDiligentImpl> m_impl;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
