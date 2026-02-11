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

    ITexture* CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count) override;
    void DestroyQueryPool(IQueryPool* pool) override;

    int GetMaxSamples(int requestedSamples) override;

    void Present() override;
    void BlitToDefaultFramebuffer(IRenderTarget* src) override;
    void BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) override;
    void Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) override;
    void BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) override;
    void BlitDepthOnlyFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst) override;

private:
    std::unique_ptr<CommandListGL> m_immediateCommandList;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
