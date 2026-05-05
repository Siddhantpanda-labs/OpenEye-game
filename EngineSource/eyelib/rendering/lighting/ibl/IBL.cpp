#include "eyelib/rendering/lighting/ibl/IBL.h"
#include "eyelib/rendering/geometry/MeshDescriptor.h"
#include "eyelib/rendering/core/PassPushConstants.h"
#include "eyelib/rendering/backend/RenderBackendPolicy.h"
#include "eyelib/rendering/resources/Shader.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/RHIUtils.h"
#include "eyelib/rendering/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/core/Log.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <optional>

namespace eyelib {
namespace rendering {

namespace {
unsigned int NativeId(rhi::ITexture* texture) {
    if (!texture) return 0;
    void* native = texture->GetNativeTexture();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0u;
}

std::shared_ptr<Mesh> CreateCubeMesh(rhi::IDevice* device) {
    MeshDescriptor desc;
    desc.vertices = {
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}
    };
    desc.indices32 = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };
    desc.indices16.clear();
    desc.indexFormat = IndexFormat::UInt32;
    desc.submeshes.clear();
    if (!device) return nullptr;
    return Mesh::Create(device, desc);
}

std::shared_ptr<Shader> CreateIrradianceShader(rhi::IDevice* device) {
    auto shader = std::make_shared<Shader>();
    rhi::DepthStencilState ds{};
    ds.depthTest = false;
    ds.depthWrite = false;
    if (!shader->Load(device, "passes/ibl/irradiance_convolution", Mesh::GetVertexLayoutRHI(),
                      rhi::PrimitiveTopology::TriangleList, &ds, rhi::PipelineLayoutKind::IBL,
                      std::optional{rhi::StandardAttachmentFormats::IBLFaceCapture()})) {
        return nullptr;
    }
    return shader;
}

std::shared_ptr<Shader> CreatePrefilterShader(rhi::IDevice* device) {
    auto shader = std::make_shared<Shader>();
    rhi::DepthStencilState ds{};
    ds.depthTest = false;
    ds.depthWrite = false;
    if (!shader->Load(device, "passes/ibl/prefilter", Mesh::GetVertexLayoutRHI(),
                      rhi::PrimitiveTopology::TriangleList, &ds, rhi::PipelineLayoutKind::IBL,
                      std::optional{rhi::StandardAttachmentFormats::IBLFaceCapture()})) {
        return nullptr;
    }
    return shader;
}

std::shared_ptr<Shader> CreateBRDFShader(rhi::IDevice* device) {
    const char* vert = R"(
#version 460 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 0) out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";
    const char* frag = R"(
#version 460 core
layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

const float PI = 3.14159265359;

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return tangent * H.x + bitangent * H.y + N * H.z;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;
    vec3 N = vec3(0.0, 0.0, 1.0);
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        if (NdotL > 0.0) {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.0001);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return vec2(A, B);
}

void main() {
    vec2 integrated = IntegrateBRDF(v_TexCoord.x, v_TexCoord.y);
    FragColor = vec4(integrated, 0.0, 1.0);
}
)";
    auto shader = std::make_shared<Shader>();
    rhi::DepthStencilState ds{};
    ds.depthTest = false;
    ds.depthWrite = false;
    if (!shader->BuildFromSource(device, vert, frag, Mesh::GetFullscreenQuadVertexLayoutRHI(),
                                 rhi::PrimitiveTopology::TriangleList, rhi::PipelineLayoutKind::PushOnly, &ds,
                                 std::optional{rhi::StandardAttachmentFormats::BrdfLutRGBA16F()}))
        return nullptr;
    return shader;
}
} // namespace

IBL::IBL() = default;

IBL::~IBL() {
    if (m_deviceRHI) {
        for (rhi::IDescriptorSet*& set : m_iblPassSets) {
            if (!set) continue;
            m_deviceRHI->EnqueueDestroyDescriptorSet(set);
            set = nullptr;
        }
        if (m_irradianceRHI) m_deviceRHI->DestroyTexture(m_irradianceRHI);
        if (m_prefilterRHI) m_deviceRHI->DestroyTexture(m_prefilterRHI);
        if (m_brdfLUTRHI) m_deviceRHI->DestroyTexture(m_brdfLUTRHI);
    }
    m_environmentRHI = nullptr;
    m_irradianceRHI = nullptr;
    m_prefilterRHI = nullptr;
    m_brdfLUTRHI = nullptr;
    m_deviceRHI = nullptr;
}

bool IBL::Initialize(rhi::ITexture* environmentCubemap, uint32_t irradianceRes, uint32_t prefilterRes) {
    if (!environmentCubemap) {
        core::Log::ErrorF("IBL::Initialize: Invalid environment cubemap");
        return false;
    }
    m_deviceRHI = m_injectedDevice;
    if (!m_deviceRHI) return false;

    if (m_irradianceRHI) { m_deviceRHI->DestroyTexture(m_irradianceRHI); m_irradianceRHI = nullptr; }
    if (m_prefilterRHI) { m_deviceRHI->DestroyTexture(m_prefilterRHI); m_prefilterRHI = nullptr; }
    if (m_brdfLUTRHI) { m_deviceRHI->DestroyTexture(m_brdfLUTRHI); m_brdfLUTRHI = nullptr; }

    m_environmentRHI = environmentCubemap;

    auto cubeMesh = CreateCubeMesh(m_deviceRHI);
    if (!cubeMesh || !cubeMesh->IsValid()) {
        core::Log::ErrorF("IBL: Failed to create cube mesh");
        return false;
    }
    auto irradianceShader = CreateIrradianceShader(m_deviceRHI);
    auto prefilterShader = CreatePrefilterShader(m_deviceRHI);
    auto brdfShader = CreateBRDFShader(m_deviceRHI);
    if (!irradianceShader || !prefilterShader || !brdfShader) {
        core::Log::ErrorF("IBL: Failed to create shaders");
        return false;
    }

    rhi::TextureDesc irradianceDesc;
    irradianceDesc.type = rhi::TextureType::TextureCube;
    irradianceDesc.width = irradianceRes;
    irradianceDesc.height = irradianceRes;
    irradianceDesc.format = rhi::RHITextureFormat::RGB16F;
    irradianceDesc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    irradianceDesc.mipLevels = 1;
    m_irradianceRHI = m_deviceRHI->CreateTexture(irradianceDesc, nullptr);

    uint32_t maxMipLevels = 1;
    uint32_t size = prefilterRes;
    while (size > 1 && maxMipLevels < 5) { size >>= 1; ++maxMipLevels; }
    rhi::TextureDesc prefilterDesc;
    prefilterDesc.type = rhi::TextureType::TextureCube;
    prefilterDesc.width = prefilterRes;
    prefilterDesc.height = prefilterRes;
    prefilterDesc.format = rhi::RHITextureFormat::RGB16F;
    prefilterDesc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    prefilterDesc.mipLevels = maxMipLevels;
    m_prefilterRHI = m_deviceRHI->CreateTexture(prefilterDesc, nullptr);

    rhi::TextureDesc brdfDesc;
    brdfDesc.type = rhi::TextureType::Texture2D;
    brdfDesc.width = 512;
    brdfDesc.height = 512;
    brdfDesc.format = rhi::RHITextureFormat::RGBA16F;
    brdfDesc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    brdfDesc.mipLevels = 1;
    m_brdfLUTRHI = m_deviceRHI->CreateTexture(brdfDesc, nullptr);

    if (!m_irradianceRHI || !m_prefilterRHI || !m_brdfLUTRHI) {
        core::Log::ErrorF("IBL: Failed to create IBL textures");
        return false;
    }

    rhi::IPipelineLayout* iblPassLayout = m_deviceRHI->GetResourceSetLayout(rhi::IDevice::ResourceSetType::IBLPass);
    if (iblPassLayout) {
        for (rhi::IDescriptorSet*& set : m_iblPassSets) {
            if (set) {
                m_deviceRHI->EnqueueDestroyDescriptorSet(set);
                set = nullptr;
            }
            set = m_deviceRHI->CreateDescriptorSet(iblPassLayout);
            if (set)
                set->UpdateTexture(0, m_environmentRHI);
        }
    }
    const uint32_t frameIndex = m_deviceRHI->GetCurrentFrameIndex() % kIblPassSetFrames;
    rhi::IDescriptorSet* activeIblSet = m_iblPassSets[frameIndex];

    rhi::ICommandList* cmd = m_deviceRHI->GetActiveRecordingCommandList();
    if (!cmd) return false;

    // Environment cubemap is sampled as SRV; avoid assuming it came from RenderTarget state here.

    const math::Mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const math::Mat4 captureViews[] = {
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3(-1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  1.0f,  0.0f), math::Vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f, -1.0f,  0.0f), math::Vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f,  1.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f, -1.0f), math::Vec3(0.0f, -1.0f,  0.0f))
    };

    // Irradiance convolution
    for (uint32_t face = 0; face < 6; ++face) {
        rhi::IRenderTarget* rt = m_deviceRHI->CreateRenderTargetFromTexture(m_irradianceRHI, face, 0);
        if (!rt) continue;
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, irradianceRes, irradianceRes);
        rhi::ClearValue clear{};
        cmd->ClearRenderTarget(rt, clear);
        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetCullFace(false);

        irradianceShader->Bind(m_deviceRHI);
        if (activeIblSet) {
            rhi::IDescriptorSet* sets[1] = { activeIblSet };
            cmd->BindDescriptorSets(0, 1, sets);
        } else {
            cmd->BindTexture(m_environmentRHI, 0);
        }
        if (backend::UseTypedPushConstants(m_deviceRHI))
            abi::WriteIblVertexPushConstants(cmd, captureProjection, captureViews[face]);
        else {
            irradianceShader->SetMat4("u_View", captureViews[face]);
            irradianceShader->SetMat4("u_Projection", captureProjection);
        }
        cubeMesh->Draw(m_deviceRHI);
        irradianceShader->Unbind();
        cmd->SetRenderTarget(nullptr);
        m_deviceRHI->DestroyRenderTarget(rt);
    }

    // Prefiltered environment map
    for (uint32_t mip = 0; mip < maxMipLevels; ++mip) {
        uint32_t mipWidth = std::max<uint32_t>(1u, prefilterRes >> mip);
        uint32_t mipHeight = mipWidth;
        float roughness = (maxMipLevels > 1) ? static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1) : 0.0f;
        for (uint32_t face = 0; face < 6; ++face) {
            rhi::IRenderTarget* rt = m_deviceRHI->CreateRenderTargetFromTexture(m_prefilterRHI, face, mip);
            if (!rt) continue;
            cmd->SetRenderTarget(rt);
            cmd->SetViewport(0, 0, mipWidth, mipHeight);
            rhi::ClearValue clear{};
            cmd->ClearRenderTarget(rt, clear);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);

            prefilterShader->Bind(m_deviceRHI);
            if (activeIblSet) {
                rhi::IDescriptorSet* sets[1] = { activeIblSet };
                cmd->BindDescriptorSets(0, 1, sets);
            } else {
                cmd->BindTexture(m_environmentRHI, 0);
            }
            if (backend::UseTypedPushConstants(m_deviceRHI)) {
                abi::WriteIblVertexPushConstants(cmd, captureProjection, captureViews[face]);
                abi::WriteIblPrefilterRoughnessPushConstant(cmd, roughness);
            } else {
                prefilterShader->SetFloat("u_Roughness", roughness);
                prefilterShader->SetMat4("u_View", captureViews[face]);
                prefilterShader->SetMat4("u_Projection", captureProjection);
            }
            cubeMesh->Draw(m_deviceRHI);
            prefilterShader->Unbind();
            cmd->SetRenderTarget(nullptr);
            m_deviceRHI->DestroyRenderTarget(rt);
        }
    }

    // BRDF LUT
    {
        rhi::IRenderTarget* rt = m_deviceRHI->CreateRenderTargetFromTexture(m_brdfLUTRHI, 0, 0);
        if (rt) {
            cmd->SetRenderTarget(rt);
            cmd->SetViewport(0, 0, brdfDesc.width, brdfDesc.height);
            rhi::ClearValue clear{};
            cmd->ClearRenderTarget(rt, clear);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            if (brdfShader->GetPipelineRHI())
                cmd->DrawFullscreenQuad(brdfShader->GetPipelineRHI());
            cmd->SetRenderTarget(nullptr);
            m_deviceRHI->DestroyRenderTarget(rt);
        }
    }

    // Upload/IBL generation writes these as render targets outside the frame graph.
    // Transition to SRV now so FrameGraph::Compile sees stable tracker state on first read.
    cmd->BarrierTexture(m_irradianceRHI, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderResource);
    cmd->BarrierTexture(m_prefilterRHI, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderResource);
    cmd->BarrierTexture(m_brdfLUTRHI, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderResource);

    return true;
}

bool IBL::GenerateIrradianceAndPrefilter(rhi::IDevice* device, rhi::ITexture* environmentCubemap,
                                         uint32_t irradianceRes, uint32_t prefilterRes,
                                         rhi::ITexture*& outIrradiance, rhi::ITexture*& outPrefilter) {
    outIrradiance = nullptr;
    outPrefilter = nullptr;
    if (!device || !environmentCubemap) return false;

    auto cubeMesh = CreateCubeMesh(device);
    auto irradianceShader = CreateIrradianceShader(device);
    auto prefilterShader = CreatePrefilterShader(device);
    if (!cubeMesh || !cubeMesh->IsValid() || !irradianceShader || !irradianceShader->IsValid() ||
        !prefilterShader || !prefilterShader->IsValid())
        return false;

    rhi::TextureDesc irradianceDesc;
    irradianceDesc.type = rhi::TextureType::TextureCube;
    irradianceDesc.width = irradianceRes;
    irradianceDesc.height = irradianceRes;
    irradianceDesc.format = rhi::RHITextureFormat::RGB16F;
    irradianceDesc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    irradianceDesc.mipLevels = 1;
    outIrradiance = device->CreateTexture(irradianceDesc, nullptr);

    uint32_t maxMipLevels = 1;
    uint32_t size = prefilterRes;
    while (size > 1 && maxMipLevels < 5) { size >>= 1; ++maxMipLevels; }
    rhi::TextureDesc prefilterDesc;
    prefilterDesc.type = rhi::TextureType::TextureCube;
    prefilterDesc.width = prefilterRes;
    prefilterDesc.height = prefilterRes;
    prefilterDesc.format = rhi::RHITextureFormat::RGB16F;
    prefilterDesc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    prefilterDesc.mipLevels = maxMipLevels;
    outPrefilter = device->CreateTexture(prefilterDesc, nullptr);

    if (!outIrradiance || !outPrefilter) {
        if (outIrradiance) { device->DestroyTexture(outIrradiance); outIrradiance = nullptr; }
        if (outPrefilter) { device->DestroyTexture(outPrefilter); outPrefilter = nullptr; }
        return false;
    }

    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (!cmd) return false;

    const math::Mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    const math::Mat4 captureViews[] = {
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3(-1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  1.0f,  0.0f), math::Vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f, -1.0f,  0.0f), math::Vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f,  1.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f, -1.0f), math::Vec3(0.0f, -1.0f,  0.0f))
    };

    for (uint32_t face = 0; face < 6; ++face) {
        rhi::IRenderTarget* rt = device->CreateRenderTargetFromTexture(outIrradiance, face, 0);
        if (!rt) continue;
        cmd->SetRenderTarget(rt);
        cmd->SetViewport(0, 0, irradianceRes, irradianceRes);
        rhi::ClearValue clear{};
        cmd->ClearRenderTarget(rt, clear);
        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetCullFace(false);
        irradianceShader->Bind(device);
        irradianceShader->SetInt("u_EnvironmentMap", 0);
        if (backend::UseTypedPushConstants(device))
            abi::WriteIblVertexPushConstants(cmd, captureProjection, captureViews[face]);
        else {
            irradianceShader->SetMat4("u_View", captureViews[face]);
            irradianceShader->SetMat4("u_Projection", captureProjection);
        }
        cmd->BindTexture(environmentCubemap, 0);
        cubeMesh->Draw(device);
        irradianceShader->Unbind();
        cmd->SetRenderTarget(nullptr);
        device->DestroyRenderTarget(rt);
    }

    for (uint32_t mip = 0; mip < maxMipLevels; ++mip) {
        uint32_t mipWidth = std::max<uint32_t>(1u, prefilterRes >> mip);
        uint32_t mipHeight = mipWidth;
        float roughness = (maxMipLevels > 1) ? static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1) : 0.0f;
        for (uint32_t face = 0; face < 6; ++face) {
            rhi::IRenderTarget* rt = device->CreateRenderTargetFromTexture(outPrefilter, face, mip);
            if (!rt) continue;
            cmd->SetRenderTarget(rt);
            cmd->SetViewport(0, 0, mipWidth, mipHeight);
            rhi::ClearValue clear{};
            cmd->ClearRenderTarget(rt, clear);
            cmd->SetDepthTest(false);
            cmd->SetDepthWrite(false);
            cmd->SetCullFace(false);
            prefilterShader->Bind(device);
            prefilterShader->SetInt("u_EnvironmentMap", 0);
            if (backend::UseTypedPushConstants(device)) {
                abi::WriteIblVertexPushConstants(cmd, captureProjection, captureViews[face]);
                abi::WriteIblPrefilterRoughnessPushConstant(cmd, roughness);
            } else {
                prefilterShader->SetFloat("u_Roughness", roughness);
                prefilterShader->SetMat4("u_View", captureViews[face]);
                prefilterShader->SetMat4("u_Projection", captureProjection);
            }
            cmd->BindTexture(environmentCubemap, 0);
            cubeMesh->Draw(device);
            prefilterShader->Unbind();
            cmd->SetRenderTarget(nullptr);
            device->DestroyRenderTarget(rt);
        }
    }

    // Generated as render targets; make them shader-readable for downstream frame-graph consumers.
    cmd->BarrierTexture(outIrradiance, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderResource);
    cmd->BarrierTexture(outPrefilter, rhi::ResourceState::RenderTarget, rhi::ResourceState::ShaderResource);

    return true;
}

unsigned int IBL::GetIrradianceMap() const { return NativeId(m_irradianceRHI); }
unsigned int IBL::GetPrefilterMap() const { return NativeId(m_prefilterRHI); }
unsigned int IBL::GetBRDFLUT() const { return NativeId(m_brdfLUTRHI); }

void IBL::BindForLighting(rhi::ICommandList* cmd, unsigned int irradianceUnit, unsigned int prefilterUnit, unsigned int brdfLUTUnit) const {
    rhi::ICommandList* list = cmd ? cmd : (m_deviceRHI ? m_deviceRHI->GetActiveRecordingCommandList() : nullptr);
    if (!list) return;
    if (m_irradianceRHI) list->BindTexture(m_irradianceRHI, irradianceUnit);
    if (m_prefilterRHI) list->BindTexture(m_prefilterRHI, prefilterUnit);
    if (m_brdfLUTRHI) list->BindTexture(m_brdfLUTRHI, brdfLUTUnit);
}

} // namespace rendering
} // namespace eyelib
