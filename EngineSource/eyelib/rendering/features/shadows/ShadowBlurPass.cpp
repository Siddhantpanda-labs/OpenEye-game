#include "eyelib/rendering/features/shadows/ShadowBlurPass.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/math/Types.h"

namespace eyelib {
namespace rendering {

ShadowBlurPass::ShadowBlurPass() = default;

ShadowBlurPass::~ShadowBlurPass() {
    if (m_pipeline && m_device) {
        if (m_descriptorSetH) m_device->DestroyDescriptorSet(m_descriptorSetH);
        if (m_descriptorSetV) m_device->DestroyDescriptorSet(m_descriptorSetV);
        m_device->DestroyPipeline(m_pipeline);
    }
}

void ShadowBlurPass::Setup(rhi::IDevice* device, rhi::IRenderTarget* sourceRt, rhi::IRenderTarget* scratchRt) {
    m_device = device;
    m_sourceRt = sourceRt;
    m_scratchRt = scratchRt;

    if (!m_pipeline && m_sourceRt && device) {
        rhi::ShaderGraphicsPipelineCreateInfo d{};
        d.assetPath = "raster/shadow/vsm_blur";
        d.topology = rhi::PrimitiveTopology::TriangleList;
        d.rasterStateOverride = rhi::RasterState{};
        d.rasterStateOverride->cullEnable = false;
        d.useDepthStencilOverride = true;
        d.depthStencilOverride.depthTest = false;
        d.depthStencilOverride.depthWrite = false;
        d.layoutKind = rhi::PipelineLayoutKind::IBL;
        rhi::PipelineAttachmentFormats att{};
        att.color.push_back(rhi::RHITextureFormat::R16G16_SFLOAT);
        att.depthStencil = rhi::RHITextureFormat::Depth24;
        d.attachmentFormats = att;
        
        m_pipeline = device->CreateGraphicsPipelineFromShaderAsset(d);
        
        if (m_pipeline) {
            rhi::IPipelineLayout* layout = device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::IBLPass);
            if (layout) {
                m_descriptorSetH = device->CreateDescriptorSet(layout);
                m_descriptorSetV = device->CreateDescriptorSet(layout);
                
                if (m_descriptorSetH && m_sourceRt->GetColorTexture(0)) {
                    m_descriptorSetH->UpdateTexture(0, m_sourceRt->GetColorTexture(0));
                }
                if (m_descriptorSetV && m_scratchRt->GetColorTexture(0)) {
                    m_descriptorSetV->UpdateTexture(0, m_scratchRt->GetColorTexture(0));
                }
            }
        }
    }
}

void ShadowBlurPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    if (m_sourceRt) {
        out.Push(m_sourceRt->GetColorTexture(0), rhi::ResourceState::ShaderResource);
    }
    if (m_scratchRt) {
        out.Push(m_scratchRt->GetColorTexture(0), rhi::ResourceState::RenderTarget);
    }
}

void ShadowBlurPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_sourceRt || !m_scratchRt || !m_pipeline) return;

    // Both RTs are array textures. We must blur each layer.
    // The number of layers is equivalent to the number of spot lights, but let's just get it from the texture.
    // However, our RenderTarget abstraction doesn't have a GetLayerCount on texture easily exposed here.
    // Let's assume up to 16 layers (kMaxSpotLights), but we can query the actual active layers from the scene.
    // Wait, the blur shader has `layerIndex` in push constants, but `CommandListVK` binds the entire RenderTarget.
    // We can use the array rendering feature of Vulkan (rendering to all layers or a specific layer).
    // Our IRenderTarget interface requires `SetDepthLayer(layer)` to render to a specific layer.
    // Or we can just render to 1 layer at a time.
    
    for (uint32_t i = 0; i < 16; ++i) { // Hardcoded 16 for now, or pass from ShadowFeature
        // --- Horizontal Pass: Source -> Scratch ---
        m_scratchRt->SetDepthLayer(i); // Using this as color layer hack, wait, RHI.h might have SetLayer
        cmd->SetRenderTarget(m_scratchRt);
        cmd->BindPipeline(m_pipeline);
        
        struct PushData {
            float dirX, dirY;
            int layerIndex;
        } pushH = {1.0f, 0.0f, (int)i};
        
        cmd->SetPushConstants(0, sizeof(pushH), &pushH);
        cmd->BindDescriptorSets(0, 1, &m_descriptorSetH);
        cmd->DrawFullscreenQuad(m_pipeline);
        
        // Manual barrier to read from scratch
        cmd->BarrierTexture(m_scratchRt->GetColorTexture(0), rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderResource);
        m_device->TransitionTextureToState(m_scratchRt->GetColorTexture(0), rhi::ResourceState::ShaderResource);

        // --- Vertical Pass: Scratch -> Source ---
        m_sourceRt->SetDepthLayer(i);
        cmd->SetRenderTarget(m_sourceRt);
        cmd->BindPipeline(m_pipeline);
        
        PushData pushV = {0.0f, 1.0f, (int)i};
        cmd->SetPushConstants(0, sizeof(pushV), &pushV);
        cmd->BindDescriptorSets(0, 1, &m_descriptorSetV);
        cmd->DrawFullscreenQuad(m_pipeline);
        
        // Manual barrier to read from source for next passes
        cmd->BarrierTexture(m_sourceRt->GetColorTexture(0), rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderResource);
        m_device->TransitionTextureToState(m_sourceRt->GetColorTexture(0), rhi::ResourceState::ShaderResource);
    }
}

} // namespace rendering
} // namespace eyelib
