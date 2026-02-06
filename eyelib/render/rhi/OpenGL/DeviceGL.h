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
    void DestroyShader(IShader* shader) override;

    IPipeline* CreateGraphicsPipeline(const PipelineDesc& desc) override;
    IPipeline* CreateComputePipeline(void* computeShader) override;
    void DestroyPipeline(IPipeline* pipeline) override;

    IRenderTarget* CreateRenderTarget(uint32_t width, uint32_t height,
                                      const std::vector<RHITextureFormat>& colorFormats,
                                      RHITextureFormat depthFormat,
                                      uint32_t samples) override;
    void DestroyRenderTarget(IRenderTarget* rt) override;

    IQueryPool* CreateQueryPool(QueryType type, uint32_t count) override;
    void DestroyQueryPool(IQueryPool* pool) override;

    void Present() override;

private:
    std::unique_ptr<CommandListGL> m_immediateCommandList;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
