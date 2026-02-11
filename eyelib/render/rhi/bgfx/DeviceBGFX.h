#pragma once

#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/bgfx/CommandListBGFX.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {
namespace rhi {

struct DeviceBGFX : IDevice {
    DeviceBGFX(void* nwh, uint32_t width, uint32_t height);
    ~DeviceBGFX() override;

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

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    /** Blit program and fullscreen quad for BlitToDefaultFramebuffer. Lazy-inited. */
    void EnsureBlitProgram();
    bool HasValidBlitProgram() const;
    void BlitToBackbufferWithProgram(IRenderTarget* src);

    /** For BGFX command list: get or create uniform by name. Returns handle idx or kInvalidIdx. */
    enum class UniformKind : uint8_t { Vec4, Mat4, Sampler };
    uint16_t GetOrCreateUniform(const char* name, UniformKind kind);

private:
    std::unique_ptr<CommandListBGFX> m_immediateCommandList;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    static constexpr uint16_t kInvalidIdx = 0xffff;
    uint16_t m_blitProgramIdx = kInvalidIdx;
    uint16_t m_fullscreenVbIdx = kInvalidIdx;
    uint16_t m_fullscreenIbIdx = kInvalidIdx;
    uint16_t m_blitTextureUniformIdx = kInvalidIdx;
    bool m_blitProgramTried = false;
    std::unordered_map<std::string, uint16_t> m_uniformHandles;
};

} // namespace rhi
} // namespace render
} // namespace eyelib
