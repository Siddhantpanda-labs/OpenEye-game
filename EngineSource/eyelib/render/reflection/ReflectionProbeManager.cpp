#include "eyelib/render/reflection/ReflectionProbeManager.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/ibl/IBL.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace eyelib {
namespace render {

ReflectionProbeManager::OwnedProbeTextures::~OwnedProbeTextures() {
    if (!device) return;
    if (cubemap) { device->DestroyTexture(cubemap); cubemap = nullptr; }
    if (irradiance) { device->DestroyTexture(irradiance); irradiance = nullptr; }
    if (prefilter) { device->DestroyTexture(prefilter); prefilter = nullptr; }
}

size_t ReflectionProbeManager::AddProbe(const ReflectionProbe& probe) {
    m_probes.push_back(probe);
    return m_probes.size() - 1;
}

ReflectionProbe* ReflectionProbeManager::GetProbe(size_t index) {
    if (index >= m_probes.size()) return nullptr;
    return &m_probes[index];
}

const ReflectionProbe* ReflectionProbeManager::GetProbe(size_t index) const {
    if (index >= m_probes.size()) return nullptr;
    return &m_probes[index];
}

ReflectionProbe* ReflectionProbeManager::FindClosestProbe(const math::Vec3& worldPos) {
    ReflectionProbe* best = nullptr;
    float bestDistSq = std::numeric_limits<float>::max();
    for (ReflectionProbe& p : m_probes) {
        if (!p.IsValid()) continue;
        math::Vec3 d = worldPos - p.GetPosition();
        float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
        float r = p.GetRadius();
        if (distSq <= r * r && distSq < bestDistSq) {
            bestDistSq = distSq;
            best = &p;
        }
    }
    return best;
}

const ReflectionProbe* ReflectionProbeManager::FindClosestProbe(const math::Vec3& worldPos) const {
    const ReflectionProbe* best = nullptr;
    float bestDistSq = std::numeric_limits<float>::max();
    for (const ReflectionProbe& p : m_probes) {
        if (!p.IsValid()) continue;
        math::Vec3 d = worldPos - p.GetPosition();
        float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
        float r = p.GetRadius();
        if (distSq <= r * r && distSq < bestDistSq) {
            bestDistSq = distSq;
            best = &p;
        }
    }
    return best;
}

ReflectionProbe* ReflectionProbeManager::GetGlobalProbe() {
    return GetProbe(0);
}

const ReflectionProbe* ReflectionProbeManager::GetGlobalProbe() const {
    return GetProbe(0);
}

void ReflectionProbeManager::SetGlobalProbeFromPrefiltered(rhi::ITexture* prefilteredMap, rhi::ITexture* irradianceMap) {
    if (!prefilteredMap) return;
    if (m_probes.empty())
        m_probes.push_back(ReflectionProbe());
    ReflectionProbe& g = m_probes[0];
    g.SetPosition(math::Vec3(0.f, 0.f, 0.f));
    g.SetRadius(10000.f);
    g.SetPrefilteredMap(prefilteredMap);
    g.SetIrradianceMap(irradianceMap);
    g.SetDynamic(false);
}

void ReflectionProbeManager::Clear() {
    m_ownedTextures.clear();
    m_probes.clear();
}

bool ReflectionProbeManager::CaptureProbe(rhi::IDevice* device, const RenderToTargetFn& renderToTarget,
                                         const RenderFrameData& frameData, size_t probeIndex,
                                         const math::Vec3& position, uint32_t resolution) {
    if (!device || !renderToTarget || resolution == 0) return false;

    while (m_probes.size() <= probeIndex)
        m_probes.push_back(ReflectionProbe());
    while (m_ownedTextures.size() <= probeIndex)
        m_ownedTextures.push_back(nullptr);

    const math::Mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    const math::Vec3 dirs[6] = {
        math::Vec3( 1.f,  0.f,  0.f), math::Vec3(-1.f,  0.f,  0.f),
        math::Vec3( 0.f,  1.f,  0.f), math::Vec3( 0.f, -1.f,  0.f),
        math::Vec3( 0.f,  0.f,  1.f), math::Vec3( 0.f,  0.f, -1.f)
    };
    const math::Vec3 ups[6] = {
        math::Vec3(0.f, -1.f,  0.f), math::Vec3(0.f, -1.f,  0.f),
        math::Vec3(0.f,  0.f,  1.f), math::Vec3(0.f,  0.f, -1.f),
        math::Vec3(0.f, -1.f,  0.f), math::Vec3(0.f, -1.f,  0.f)
    };

    rhi::TextureDesc cubeDesc;
    cubeDesc.type = rhi::TextureType::TextureCube;
    cubeDesc.width = resolution;
    cubeDesc.height = resolution;
    cubeDesc.format = rhi::RHITextureFormat::RGB16F;
    cubeDesc.usage = rhi::TextureUsage::ShaderResource | rhi::TextureUsage::RenderTarget;
    cubeDesc.mipLevels = 1;
    rhi::ITexture* cubemap = device->CreateTexture(cubeDesc, nullptr);
    if (!cubemap) return false;

    rhi::TextureDesc depthDesc;
    depthDesc.type = rhi::TextureType::Texture2D;
    depthDesc.width = resolution;
    depthDesc.height = resolution;
    depthDesc.format = rhi::RHITextureFormat::Depth24;
    depthDesc.usage = rhi::TextureUsage::RenderTarget;
    depthDesc.mipLevels = 1;
    rhi::ITexture* depthTex = device->CreateTexture(depthDesc, nullptr);
    if (!depthTex) {
        device->DestroyTexture(cubemap);
        return false;
    }

    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd) {
        device->DestroyTexture(depthTex);
        device->DestroyTexture(cubemap);
        return false;
    }

    for (uint32_t face = 0; face < 6; ++face) {
        math::Mat4 view = glm::lookAt(position, position + dirs[face], ups[face]);
        CameraView faceCam;
        faceCam.view = view;
        faceCam.projection = captureProjection;
        faceCam.viewPos = position;
        faceCam.nearPlane = 0.1f;
        faceCam.farPlane = 100.0f;

        rhi::IRenderTarget* rt = device->CreateRenderTargetFromTextureWithDepth(cubemap, face, 0, depthTex);
        if (!rt) continue;
        renderToTarget(cmd, frameData, faceCam, rt);
        device->DestroyRenderTarget(rt);
    }

    device->DestroyTexture(depthTex);

    rhi::ITexture* irradiance = nullptr;
    rhi::ITexture* prefilter = nullptr;
    if (!IBL::GenerateIrradianceAndPrefilter(device, cubemap, 32, 128, irradiance, prefilter)) {
        device->DestroyTexture(cubemap);
        return false;
    }

    auto owned = std::make_unique<OwnedProbeTextures>();
    owned->device = device;
    owned->cubemap = cubemap;
    owned->irradiance = irradiance;
    owned->prefilter = prefilter;
    m_ownedTextures[probeIndex] = std::move(owned);

    ReflectionProbe& p = m_probes[probeIndex];
    p.SetPosition(position);
    p.SetRadius(50.f);
    p.SetCubemap(m_ownedTextures[probeIndex]->cubemap);
    p.SetPrefilteredMap(m_ownedTextures[probeIndex]->prefilter);
    p.SetIrradianceMap(m_ownedTextures[probeIndex]->irradiance);
    p.SetDynamic(false);
    return true;
}

} // namespace render
} // namespace eyelib
