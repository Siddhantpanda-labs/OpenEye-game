#include "eyelib/rendering/rhi/Vulkan/pipelines/PipelineVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanGlslToSpirv.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanResourceSpaces.h"
#include "eyelib/rendering/rhi/Vulkan/core/DeviceVK.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/ShaderVK.h"
#include "eyelib/rendering/rhi/Vulkan/utils/VulkanEnums.h"
#include "eyelib/rendering/rhi/Vulkan/pipelines/VulkanPipelineHash.h"
#include "eyelib/rendering/rhi/Vulkan/utils/DebugUtilsVK.h"
#include "eyelib/rendering/shaders/ShaderPathResolve.h"
#include "eyelib/rendering/shaders/ShaderSourceLoader.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/core/Log.h"
#include <vulkan/vulkan.h>
#include <atomic>
#include <cassert>
#include <vector>
#include <string>

namespace eyelib {
namespace rendering {
namespace rhi {

PipelineVK::PipelineVK(DeviceVK* device) : m_device(device) {}

PipelineVK::~PipelineVK() {
    if (!m_pipeline || !m_device || !m_device->GetDevice()) return;
    if (m_graphicsCacheHash != 0) {
        // Graphics: shared VkPipeline — refcounted via DeviceVK (DestroyPipeline normally clears m_pipeline first).
        static_cast<DeviceVK*>(m_device)->ReleaseGraphicsPipeline(m_graphicsCacheHash, m_pipeline);
    } else {
        vkDestroyPipeline(static_cast<VkDevice>(m_device->GetDevice()), static_cast<VkPipeline>(m_pipeline), nullptr);
    }
    m_pipeline = nullptr;
}

void PipelineVK::GetComputeBlockPresence(ComputeBlockPresence& out) const { out = m_computeBlockPresence; }

namespace {

static std::string ApplyKeywordDefinesString(const std::string& source, const std::vector<std::string>& keywords) {
    if (keywords.empty()) return source;
    std::string prefix;
    for (const auto& k : keywords) {
        if (k.empty()) continue;
        prefix += "#define ";
        prefix += k;
        prefix += " 1\n";
    }
    return prefix + source;
}

/** Mesh graphics with real descriptor sets uses the same `VkPipelineLayout` kinds as vertex passes (not compute layouts). */
static bool PipelineLayoutKindSupportsMeshWithResourceSpace(PipelineLayoutKind kind) {
    switch (kind) {
    case PipelineLayoutKind::Forward:
    case PipelineLayoutKind::Post:
    case PipelineLayoutKind::Shadow:
    case PipelineLayoutKind::GBuffer:
    case PipelineLayoutKind::DeferredLighting:
    case PipelineLayoutKind::IBL:
    case PipelineLayoutKind::OutlinePost:
    case PipelineLayoutKind::ComposeBootstrap:
        return true;
    case PipelineLayoutKind::PushOnly:
    case PipelineLayoutKind::ComputeLightCull:
    case PipelineLayoutKind::ComputeGpuCull:
    default:
        return false;
    }
}

struct RenderingFormatSelection {
    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
};

static void ApplyExplicitAttachmentFormats(const PipelineAttachmentFormats& af, RenderingFormatSelection& out) {
    out.colorFormats.clear();
    out.depthFormat = VK_FORMAT_UNDEFINED;
    out.stencilFormat = VK_FORMAT_UNDEFINED;
    for (RHITextureFormat f : af.color)
        out.colorFormats.push_back(ToVkFormat(f));
    if (af.depthStencil == RHITextureFormat::Depth24Stencil8) {
        out.depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        out.stencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    } else if (af.depthStencil != RHITextureFormat::None) {
        out.depthFormat = ToVkFormat(af.depthStencil);
        out.stencilFormat = VK_FORMAT_UNDEFINED;
    }
}

static void InferAttachmentFormatsFromLayoutKind(PipelineLayoutKind kind, RenderingFormatSelection& out) {
    out.colorFormats.clear();
    out.depthFormat = VK_FORMAT_UNDEFINED;
    out.stencilFormat = VK_FORMAT_UNDEFINED;
    switch (kind) {
    case PipelineLayoutKind::Shadow:
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::Forward:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::GBuffer:
        out.colorFormats = {
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R16G16B16A16_SFLOAT,
        };
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::DeferredLighting:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        out.depthFormat = VK_FORMAT_D32_SFLOAT;
        break;
    case PipelineLayoutKind::IBL:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        break;
    case PipelineLayoutKind::OutlinePost:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        break;
    case PipelineLayoutKind::ComposeBootstrap:
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        out.depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        out.stencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;
        break;
    case PipelineLayoutKind::Post: {
        static bool s_warnedPostInfer = false;
        if (!s_warnedPostInfer) {
            core::Log::Warn(
                "Vulkan: Post pipeline created without explicit PipelineDesc::attachmentFormats; defaulting to HDR "
                "RGB16F. Pass StandardAttachmentFormats from the load site for LDR / R8 / BGRA targets.");
            s_warnedPostInfer = true;
        }
        out.colorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT };
        break;
    }
    case PipelineLayoutKind::ComputeLightCull:
    case PipelineLayoutKind::ComputeGpuCull:
        break;
    case PipelineLayoutKind::PushOnly:
    default:
        out.colorFormats = { VK_FORMAT_R8G8B8A8_UNORM };
        break;
    }
}

static RenderingFormatSelection SelectRenderingFormats(const PipelineDesc& desc) {
    RenderingFormatSelection out;
    if (desc.attachmentFormats.has_value())
        ApplyExplicitAttachmentFormats(*desc.attachmentFormats, out);
    else
        InferAttachmentFormatsFromLayoutKind(desc.layoutKind, out);
    return out;
}

/** Phase 6.1: reject **`VkPipelineRenderingCreateInfo`** / blend mismatches before **`vkCreateGraphicsPipelines`**. */
static bool ValidatePipelineRenderingCreateInfoAgainstDesc(DeviceVK* device, const PipelineDesc& desc,
                                                            const RenderingFormatSelection& sel) {
    const char* dbg = desc.debugName ? desc.debugName : "(no debugName)";
    VkPhysicalDevice phys = static_cast<VkPhysicalDevice>(device->GetPhysicalDevice());
    if (phys == VK_NULL_HANDLE) {
        core::Log::ErrorF("CreateVkGraphicsPipeline: no physical device (name=%s)", dbg);
        return false;
    }
    VkPhysicalDeviceProperties props = {};
    vkGetPhysicalDeviceProperties(phys, &props);
    const uint32_t maxCA = props.limits.maxColorAttachments;
    const uint32_t maxFO = props.limits.maxFragmentOutputAttachments;
    const size_t colorCount = sel.colorFormats.size();

    if (colorCount > maxCA) {
        core::Log::ErrorF(
            "CreateVkGraphicsPipeline: color attachment count %zu exceeds maxColorAttachments (%u) (VkPipelineRenderingCreateInfo); name=%s",
            colorCount, maxCA, dbg);
        return false;
    }
    if (colorCount > maxFO) {
        core::Log::ErrorF(
            "CreateVkGraphicsPipeline: color attachment count %zu exceeds maxFragmentOutputAttachments (%u); name=%s",
            colorCount, maxFO, dbg);
        return false;
    }
    for (size_t i = 0; i < colorCount; ++i) {
        if (sel.colorFormats[i] == VK_FORMAT_UNDEFINED) {
            core::Log::ErrorF(
                "CreateVkGraphicsPipeline: color attachment[%zu] is VK_FORMAT_UNDEFINED (check PipelineDesc::attachmentFormats); name=%s",
                i, dbg);
            return false;
        }
    }

    if (desc.attachmentFormats.has_value()) {
        const PipelineAttachmentFormats& af = *desc.attachmentFormats;
        if (af.color.size() != colorCount) {
            core::Log::ErrorF(
                "CreateVkGraphicsPipeline: internal error: attachmentFormats.color size %zu != resolved color count %zu; name=%s",
                af.color.size(), colorCount, dbg);
            return false;
        }
        for (size_t i = 0; i < af.color.size(); ++i) {
            const RHITextureFormat cf = af.color[i];
            if (cf == RHITextureFormat::None) {
                core::Log::ErrorF(
                    "CreateVkGraphicsPipeline: PipelineDesc::attachmentFormats.color[%zu] is None (illegal); name=%s", i, dbg);
                return false;
            }
            if (cf == RHITextureFormat::Depth24 || cf == RHITextureFormat::Depth24Stencil8) {
                core::Log::ErrorF(
                    "CreateVkGraphicsPipeline: attachmentFormats.color[%zu] must not be a depth/stencil RHITextureFormat; name=%s",
                    i, dbg);
                return false;
            }
        }
    }

    if (sel.depthFormat == VK_FORMAT_UNDEFINED && sel.stencilFormat != VK_FORMAT_UNDEFINED) {
        core::Log::ErrorF(
            "CreateVkGraphicsPipeline: stencilAttachmentFormat set without depthAttachmentFormat (unsupported); name=%s",
            dbg);
        return false;
    }

    return true;
}

} // namespace

void* CreateVkGraphicsPipeline(DeviceVK* device, const PipelineDesc& desc, void* pipelineLayout, void* pipelineCache) {
    if (desc.meshShader)
        return CreateVkMeshGraphicsPipeline(device, desc, pipelineLayout, pipelineCache);
    if (!device || !desc.vertexShader || !desc.fragmentShader || !pipelineLayout)
        return nullptr;
    ShaderVK* vs = static_cast<ShaderVK*>(desc.vertexShader);
    ShaderVK* fs = static_cast<ShaderVK*>(desc.fragmentShader);
    if (!vs->m_module || !fs->m_module) return nullptr;

    const RenderingFormatSelection selectedFormats = SelectRenderingFormats(desc);
    if (!ValidatePipelineRenderingCreateInfoAgainstDesc(device, desc, selectedFormats))
        return nullptr;

    const char* dbg = desc.debugName ? desc.debugName : "(no debugName)";
    DepthStencilState depthState = desc.depthStencil;
    if (selectedFormats.depthFormat == VK_FORMAT_UNDEFINED) {
        if (depthState.depthTest || depthState.depthWrite) {
            static std::atomic<int> s_depthClampWarns{0};
            const int n = s_depthClampWarns.fetch_add(1);
            if (n < 32) {
                core::Log::WarnF(
                    "CreateVkGraphicsPipeline: depth test/write in PipelineDesc but VkPipelineRenderingCreateInfo has no depth "
                    "attachment (layoutKind=%u); clamping depth test/write off. Prefer explicit attachmentFormats + matching depth "
                    "state. name=%s",
                    static_cast<unsigned>(static_cast<uint32_t>(desc.layoutKind)), dbg);
            }
            depthState.depthTest = false;
            depthState.depthWrite = false;
        }
    }
    if (selectedFormats.stencilFormat == VK_FORMAT_UNDEFINED && depthState.stencilTest) {
        static std::atomic<int> s_stencilClampWarns{0};
        const int n = s_stencilClampWarns.fetch_add(1);
        if (n < 16) {
            core::Log::WarnF(
                "CreateVkGraphicsPipeline: stencil test in PipelineDesc but no stencilAttachmentFormat in VkPipelineRenderingCreateInfo; "
                "clamping stencil off. name=%s",
                dbg);
        }
        depthState.stencilTest = false;
    }

    VkDevice vkDevice = static_cast<VkDevice>(device->GetDevice());
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pipelineLayout);
    VkPipelineCache cache = static_cast<VkPipelineCache>(pipelineCache);

    const char* entryName = "main";
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = static_cast<VkShaderModule>(vs->m_module);
    stages[0].pName = entryName;
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = static_cast<VkShaderModule>(fs->m_module);
    stages[1].pName = entryName;

    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attrs;
    if (!desc.vertexLayout.attributes.empty() && desc.vertexLayout.strideInBytes > 0) {
        VkVertexInputBindingDescription binding = {};
        binding.binding = 0;
        binding.stride = desc.vertexLayout.strideInBytes;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(binding);
        for (const auto& a : desc.vertexLayout.attributes) {
            VkVertexInputAttributeDescription attr = {};
            attr.location = a.location;
            attr.binding = 0;
            attr.format = ToVkVertexFormat(a.type);
            attr.offset = a.offsetInBytes;
            attrs.push_back(attr);
        }
    }
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = ToVkPrimitiveTopology(desc.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster = {};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = desc.raster.cullEnable ? (desc.raster.frontCounterClockwise ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT) : VK_CULL_MODE_NONE;
    raster.frontFace = desc.raster.frontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    raster.depthBiasEnable = desc.raster.depthBiasEnable ? VK_TRUE : VK_FALSE;
    raster.depthBiasConstantFactor = desc.raster.depthBiasConstant;
    raster.depthBiasSlopeFactor = desc.raster.depthBiasSlope;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Single source of truth for dynamic rendering: blend attachment count MUST equal colorAttachmentCount (Vulkan spec).
    const uint32_t colorAttachmentCount = static_cast<uint32_t>(selectedFormats.colorFormats.size());
    std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(colorAttachmentCount);
    for (uint32_t i = 0; i < colorAttachmentCount; ++i) {
        VkPipelineColorBlendAttachmentState& a = colorBlendAttachments[i];
        a.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        a.blendEnable = desc.blend.blendEnable ? VK_TRUE : VK_FALSE;
        a.srcColorBlendFactor = ToVkBlendFactor(desc.blend.srcColor);
        a.dstColorBlendFactor = ToVkBlendFactor(desc.blend.dstColor);
        a.colorBlendOp = ToVkBlendOp(desc.blend.colorOp);
        a.srcAlphaBlendFactor = ToVkBlendFactor(desc.blend.srcAlpha);
        a.dstAlphaBlendFactor = ToVkBlendFactor(desc.blend.dstAlpha);
        a.alphaBlendOp = ToVkBlendOp(desc.blend.alphaOp);
    }

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = colorAttachmentCount;
    colorBlend.pAttachments = colorAttachmentCount ? colorBlendAttachments.data() : nullptr;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = depthState.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = depthState.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = ToVkCompareOp(depthState.depthCompare);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = depthState.stencilTest ? VK_TRUE : VK_FALSE;
    depthStencil.front = depthStencil.back = { VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0, 0, 0 };

    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = colorAttachmentCount;
    renderingInfo.pColorAttachmentFormats = colorAttachmentCount ? selectedFormats.colorFormats.data() : nullptr;
    renderingInfo.depthAttachmentFormat = selectedFormats.depthFormat;
    renderingInfo.stencilAttachmentFormat = selectedFormats.stencilFormat;
    assert(colorBlend.attachmentCount == renderingInfo.colorAttachmentCount);

#if defined(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
    VkGraphicsPipelineLibraryCreateInfoEXT graphicsPipelineLibraryInfo = {};
    if (device->SupportsGraphicsPipelineLibrary()) {
        graphicsPipelineLibraryInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT;
        graphicsPipelineLibraryInfo.pNext = &renderingInfo;
        graphicsPipelineLibraryInfo.flags =
            VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT |
            VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT |
            VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT |
            VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;
    }
#endif

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    if (device->SupportsExtendedDynamicState()) {
        dynamicStates.push_back(VK_DYNAMIC_STATE_CULL_MODE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE);
        dynamicStates.push_back(VK_DYNAMIC_STATE_DEPTH_COMPARE_OP);
    }
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo gpInfo = {};
    gpInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
#if defined(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_LIBRARY_CREATE_INFO_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT) && defined(VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT)
    gpInfo.pNext = device->SupportsGraphicsPipelineLibrary() ? static_cast<void*>(&graphicsPipelineLibraryInfo) : static_cast<void*>(&renderingInfo);
#else
    gpInfo.pNext = &renderingInfo;
#endif
    gpInfo.stageCount = 2;
    gpInfo.pStages = stages;
    gpInfo.pVertexInputState = &vertexInput;
    gpInfo.pInputAssemblyState = &inputAssembly;
    gpInfo.pViewportState = &viewportState;
    gpInfo.pRasterizationState = &raster;
    gpInfo.pMultisampleState = &multisample;
    gpInfo.pColorBlendState = &colorBlend;
    gpInfo.pDepthStencilState = &depthStencil;
    gpInfo.pDynamicState = &dynamicState;
    gpInfo.layout = layout;
    gpInfo.renderPass = VK_NULL_HANDLE;
    gpInfo.subpass = 0;
    gpInfo.basePipelineHandle = VK_NULL_HANDLE;
    gpInfo.basePipelineIndex = -1;
#if defined(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (device->SupportsDescriptorBuffer())
        gpInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
#endif

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateGraphicsPipelines(vkDevice, cache, 1, &gpInfo, nullptr, &pipeline);
    if (res != VK_SUCCESS || pipeline == VK_NULL_HANDLE) {
        core::Log::ErrorF(
            "vkCreateGraphicsPipelines failed: VkResult=%d (0x%08X) layoutKind=%u colorAttachments=%u name=%s",
            static_cast<int>(res), static_cast<unsigned int>(static_cast<uint32_t>(res)),
            static_cast<unsigned>(static_cast<uint32_t>(desc.layoutKind)), colorAttachmentCount, dbg);
        return nullptr;
    }
    device->DeveloperLogPipelineExecutablePropertiesOnce(pipeline, desc.debugName);
    return pipeline;
}

void* CreateVkComputePipeline(DeviceVK* device, void* pipelineLayout, void* pipelineCache, void* shaderModule) {
    if (!device || !pipelineLayout || !shaderModule) return nullptr;
    VkDevice vkDevice = static_cast<VkDevice>(device->GetDevice());
    VkPipelineLayout layout = static_cast<VkPipelineLayout>(pipelineLayout);
    VkPipelineCache cache = static_cast<VkPipelineCache>(pipelineCache);
    VkShaderModule module = static_cast<VkShaderModule>(shaderModule);
    if (vkDevice == VK_NULL_HANDLE || layout == VK_NULL_HANDLE || module == VK_NULL_HANDLE)
        return nullptr;

    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = module;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.stage = stageInfo;
    createInfo.layout = layout;
#if defined(VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT)
    if (device->SupportsDescriptorBuffer())
        createInfo.flags |= VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
#endif

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult res = vkCreateComputePipelines(vkDevice, cache, 1, &createInfo, nullptr, &pipeline);
    if (res != VK_SUCCESS || pipeline == VK_NULL_HANDLE) {
        core::Log::ErrorF(
            "vkCreateComputePipelines failed: VkResult=%d (0x%08X)",
            static_cast<int>(res), static_cast<unsigned int>(static_cast<uint32_t>(res)));
        return nullptr;
    }
    device->DeveloperLogPipelineExecutablePropertiesOnce(pipeline, "compute");
    return pipeline;
}

IPipeline* CreateGraphicsPipelineVK(DeviceVK* device, const PipelineDesc& desc) {
    if (!device) return nullptr;
    const bool meshPath = desc.meshShader != nullptr;
    if (meshPath && desc.vertexShader) {
        core::Log::ErrorF("CreateGraphicsPipelineVK: cannot set both meshShader and vertexShader (name=%s)",
                          desc.debugName ? desc.debugName : "?");
        return nullptr;
    }
    if (meshPath && desc.layoutKind != PipelineLayoutKind::PushOnly) {
        if (!device->SupportsMeshShader()) {
            core::Log::ErrorF(
                "CreateGraphicsPipelineVK: mesh pipeline with resource-space layout requires VK_EXT_mesh_shader (name=%s)",
                desc.debugName ? desc.debugName : "?");
            return nullptr;
        }
        if (!PipelineLayoutKindSupportsMeshWithResourceSpace(desc.layoutKind)) {
            core::Log::ErrorF(
                "CreateGraphicsPipelineVK: mesh pipelines use PushOnly or a graphics pass layout (Forward, GBuffer, Post, Shadow, "
                "DeferredLighting, IBL, OutlinePost); layoutKind=%u name=%s",
                static_cast<unsigned>(static_cast<uint32_t>(desc.layoutKind)), desc.debugName ? desc.debugName : "?");
            return nullptr;
        }
    }
    const uint64_t hash = HashPipelineDesc(desc);
    void* pipeline = device->GetCachedGraphicsPipeline(hash);
    void* layoutToUse = nullptr;
    if (meshPath) {
        if (desc.layoutKind == PipelineLayoutKind::PushOnly) {
            layoutToUse = device->GetMeshPushPipelineLayout();
            if (!layoutToUse) {
                core::Log::ErrorF(
                    "CreateGraphicsPipelineVK: PushOnly mesh pipeline requires GetMeshPushPipelineLayout() (device init / extension). name=%s",
                    desc.debugName ? desc.debugName : "?");
                return nullptr;
            }
        } else {
            layoutToUse = device->GetPipelineLayoutForKind(desc.layoutKind);
            if (!layoutToUse) {
                core::Log::ErrorF("CreateGraphicsPipelineVK: mesh pipeline missing VkPipelineLayout for layoutKind=%u name=%s",
                                  static_cast<unsigned>(static_cast<uint32_t>(desc.layoutKind)), desc.debugName ? desc.debugName : "?");
                return nullptr;
            }
        }
    } else if (desc.layoutKind != PipelineLayoutKind::PushOnly) {
        layoutToUse = device->GetPipelineLayoutForKind(desc.layoutKind);
    }
    if (!layoutToUse && !meshPath) {
        layoutToUse = device->GetDefaultPipelineLayout();
    }
    const bool canCreateClassic = !meshPath && desc.vertexShader && desc.fragmentShader;
    const bool canCreateMesh = meshPath && desc.meshShader && desc.fragmentShader;
    // Pipeline cache is optional: VK_NULL_HANDLE is valid for vkCreateGraphicsPipelines (no serialized cache).
    if (!pipeline && (canCreateClassic || canCreateMesh) && layoutToUse) {
        void* created = CreateVkGraphicsPipeline(device, desc, layoutToUse, device->GetPipelineCache());
        if (created) {
            if (desc.debugName)
                SetDebugNamePipeline(device, created, desc.debugName);
            device->RegisterGraphicsPipeline(hash, created);
            pipeline = created;
        }
    }
    if (!pipeline) {
        static int s_logCount = 0;
        if (s_logCount < 8) {
            ++s_logCount;
            const char* dbg = desc.debugName ? desc.debugName : "(no debugName)";
            core::Log::WarnF(
                "CreateGraphicsPipelineVK: graphics pipeline creation failed or skipped (layoutKind=%u layout=%p vs=%p mesh=%p fs=%p name=%s).",
                static_cast<unsigned>(static_cast<int>(desc.layoutKind)), layoutToUse, (void*)desc.vertexShader,
                (void*)desc.meshShader, (void*)desc.fragmentShader, dbg);
        }
        return nullptr;
    }
    auto* p = new PipelineVK(device);
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = desc.topology;
    p->m_patchVertexCount = desc.patchVertexCount;
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;
    p->m_pipeline = pipeline;
    p->m_layoutKind = desc.layoutKind;
    p->m_nativePipelineLayout =
        (desc.layoutKind != PipelineLayoutKind::PushOnly && layoutToUse) ? layoutToUse : nullptr;
    p->m_requiredSetCount = static_cast<uint8_t>(
        (meshPath && desc.layoutKind == PipelineLayoutKind::PushOnly)
            ? 0u
            : vkres::GetRequiredDescriptorSetCount(desc.layoutKind));
    p->m_usesMeshShaders = meshPath;
    p->m_graphicsCacheHash = hash;
    device->RetainGraphicsPipeline(hash);
    return p;
}

IPipeline* CreateGraphicsPipelineFromShaderAssetVK(DeviceVK* device, const ShaderGraphicsPipelineCreateInfo& info) {
    if (!device) return nullptr;
    const bool fromOverrides = !info.vertexSourceOverride.empty() && !info.fragmentSourceOverride.empty();
    if (!fromOverrides && info.assetPath.empty()) return nullptr;

    std::string root = platform::filesystem::GetEngineAssetsRoot();
    const std::string baseDir = root.empty()
        ? std::string("assets/shaders/vulkan/")
        : platform::filesystem::NormalizePath(root + "/assets/shaders/vulkan/");

    std::string logicalName;
    const char* debugNameCstr = nullptr;
    std::string debugNameStorage;
    if (fromOverrides) {
        debugNameStorage = info.assetPath.empty() ? std::string("(generated)") : info.assetPath;
        debugNameCstr = debugNameStorage.c_str();
        logicalName = info.assetPath.empty() ? std::string("generated") : ShaderPathToLogicalName(info.assetPath);
    } else {
        logicalName = ShaderPathToLogicalName(info.assetPath);
        if (logicalName.empty()) return nullptr;
        debugNameStorage = info.assetPath;
        debugNameCstr = debugNameStorage.c_str();
    }

    IShader* vs = nullptr;
    IShader* fs = nullptr;

    auto destroyStages = [&]() {
        if (vs) DestroyShaderVK(device, vs);
        if (fs) DestroyShaderVK(device, fs);
        vs = nullptr;
        fs = nullptr;
    };

    auto tryLoadSpirvFiles = [&]() -> bool {
        destroyStages();
        const std::string vertSpv = baseDir + logicalName + ".vert.spv";
        const std::string fragSpv = baseDir + logicalName + ".frag.spv";
        return TryLoadShaderPairVK(device, vertSpv.c_str(), fragSpv.c_str(), vs, fs) && vs && fs;
    };

    auto compilePair = [&](const std::string& vsrc, const std::string& fsrc) -> bool {
        destroyStages();
        const std::string vertLabel = baseDir + logicalName + ".vert";
        const std::string fragLabel = baseDir + logicalName + ".frag";
        std::vector<uint32_t> vspirv, fspirv;
        if (!VulkanCompileGlslToSpirv(vsrc.c_str(), vsrc.size(), ShaderStage::Vertex, vertLabel.c_str(), vspirv,
                                      logicalName.c_str()))
            return false;
        if (!VulkanCompileGlslToSpirv(fsrc.c_str(), fsrc.size(), ShaderStage::Fragment, fragLabel.c_str(), fspirv,
                                      logicalName.c_str()))
            return false;
        vs = CreateShaderFromBinaryVK(device, ShaderStage::Vertex, vspirv.data(), vspirv.size() * sizeof(uint32_t));
        fs = CreateShaderFromBinaryVK(device, ShaderStage::Fragment, fspirv.data(), fspirv.size() * sizeof(uint32_t));
        return vs && fs;
    };

    if (fromOverrides) {
        const std::string vsrc = ApplyKeywordDefinesString(info.vertexSourceOverride, info.keywordDefines);
        const std::string fsrc = ApplyKeywordDefinesString(info.fragmentSourceOverride, info.keywordDefines);
        if (!compilePair(vsrc, fsrc)) {
            destroyStages();
            core::Log::ErrorF("CreateGraphicsPipelineFromShaderAssetVK: GLSL compile failed for %s", debugNameCstr);
            return nullptr;
        }
    } else {
        switch (info.sourcePolicy) {
        case ShaderSourcePolicy::SpirvOnly:
            if (!tryLoadSpirvFiles()) {
                destroyStages();
                core::Log::ErrorF("CreateGraphicsPipelineFromShaderAssetVK: SpirvOnly but missing or invalid .spv for %s",
                                  info.assetPath.c_str());
                return nullptr;
            }
            break;
        case ShaderSourcePolicy::CompileFromSource: {
            const std::string vertGlsl = baseDir + logicalName + ".vert";
            const std::string fragGlsl = baseDir + logicalName + ".frag";
            std::string vraw, fraw;
            if (!LoadShaderSource(vertGlsl, vraw) || !LoadShaderSource(fragGlsl, fraw)) {
                destroyStages();
                core::Log::ErrorF("CreateGraphicsPipelineFromShaderAssetVK: failed to load GLSL for %s",
                                  info.assetPath.c_str());
                return nullptr;
            }
            const std::string vsrc = ApplyKeywordDefinesString(vraw, info.keywordDefines);
            const std::string fsrc = ApplyKeywordDefinesString(fraw, info.keywordDefines);
            if (!compilePair(vsrc, fsrc)) {
                destroyStages();
                core::Log::ErrorF("CreateGraphicsPipelineFromShaderAssetVK: GLSL compile failed for %s", debugNameCstr);
                return nullptr;
            }
            break;
        }
        case ShaderSourcePolicy::PreferSpirv:
        default:
            if (!tryLoadSpirvFiles()) {
                const std::string vertGlsl = baseDir + logicalName + ".vert";
                const std::string fragGlsl = baseDir + logicalName + ".frag";
                std::string vraw, fraw;
                if (!LoadShaderSource(vertGlsl, vraw) || !LoadShaderSource(fragGlsl, fraw)) {
                    destroyStages();
                    core::Log::ErrorF(
                        "CreateGraphicsPipelineFromShaderAssetVK: no .spv and missing GLSL for %s",
                        info.assetPath.c_str());
                    return nullptr;
                }
                const std::string vsrc = ApplyKeywordDefinesString(vraw, info.keywordDefines);
                const std::string fsrc = ApplyKeywordDefinesString(fraw, info.keywordDefines);
                if (!compilePair(vsrc, fsrc)) {
                    destroyStages();
                    core::Log::ErrorF("CreateGraphicsPipelineFromShaderAssetVK: GLSL compile failed for %s", debugNameCstr);
                    return nullptr;
                }
            }
            break;
        }
    }

    PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.debugName = debugNameCstr;
    desc.vertexLayout = info.vertexLayout;
    desc.topology = info.topology;
    desc.layoutKind = info.layoutKind;
    desc.attachmentFormats = info.attachmentFormats;
    if (info.useDepthStencilOverride)
        desc.depthStencil = info.depthStencilOverride;
    else {
        desc.depthStencil.depthTest = true;
        desc.depthStencil.depthWrite = true;
    }
    IPipeline* pipe = CreateGraphicsPipelineVK(device, desc);
    destroyStages();
    return pipe;
}

IPipeline* CreateComputePipelineVK(DeviceVK* device, void* computeShader) {
    return CreateComputePipelineVK(device, computeShader, PipelineLayoutKind::PushOnly);
}

IPipeline* CreateComputePipelineVK(DeviceVK* device, void* computeShader, PipelineLayoutKind layoutKind) {
    if (!device || !computeShader) return nullptr;
    void* layoutToUse = nullptr;
    if (layoutKind != PipelineLayoutKind::PushOnly) {
        layoutToUse = device->GetPipelineLayoutForKind(layoutKind);
    }
    if (!layoutToUse) {
        layoutToUse = device->GetComputePipelineLayout();
    }
    if (!layoutToUse) return nullptr;
    ShaderVK* shader = static_cast<ShaderVK*>(computeShader);
    if (!shader->m_module) return nullptr;
    void* vkPipe = CreateVkComputePipeline(device, layoutToUse, device->GetPipelineCache(), shader->m_module);
    if (!vkPipe) {
        static int s_logCount = 0;
        if (s_logCount < 8) {
            ++s_logCount;
            core::Log::WarnF("CreateComputePipelineVK: VkPipeline is null (layoutKind=%u).", static_cast<unsigned>(static_cast<int>(layoutKind)));
        }
        return nullptr;
    }
    auto* p = new PipelineVK(device);
    p->m_isCompute = true;
    p->m_layoutKind = layoutKind;
    p->m_requiredSetCount = static_cast<uint8_t>(vkres::GetRequiredDescriptorSetCount(layoutKind));
    p->m_pipeline = vkPipe;
    p->m_nativePipelineLayout = (layoutKind != PipelineLayoutKind::PushOnly) ? layoutToUse : nullptr;
    return p;
}

} // namespace rhi
} // namespace rendering
} // namespace eyelib
