#include "eyelib/rendering/lighting/ibl/IblDefaultProbes.h"
#include "eyelib/rendering/lighting/ibl/IblCpuDefaults.h"
#include "eyelib/rendering/lighting/ibl/IBL.h"

#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

#include <chrono>
#include <vector>

namespace eyelib {
namespace rendering {
namespace {

constexpr uint32_t kFallbackBrdfLutDim = 256u;
constexpr uint32_t kFallbackBrdfLutSamples = 64u;

static bool TryLoadBrdfLutRawBytes(std::vector<uint8_t>& outBytes) {
    const std::string root = platform::filesystem::GetEngineAssetsRoot();
    if (root.empty())
        return false;
    const char* candidates[] = {
        "assets/textures/rendering/ibl/brdf_lut_rg16f_256x256.bin",
        "assets/textures/ibl/brdf_lut_rg16f_256x256.bin",
    };
    for (const char* rel : candidates) {
        const std::string absPath = platform::filesystem::NormalizePath(root + "/" + rel);
        if (!platform::filesystem::FileExists(absPath))
            continue;
        outBytes = platform::filesystem::ReadFile(absPath);
        const size_t expected = static_cast<size_t>(kFallbackBrdfLutDim) * static_cast<size_t>(kFallbackBrdfLutDim) * 8u;
        if (outBytes.size() == expected) {
            core::Log::InfoF("IblDefaultProbes: loaded BRDF LUT asset (%u bytes) from %s.",
                             static_cast<unsigned>(outBytes.size()), absPath.c_str());
            return true;
        }
        core::Log::WarnF("IblDefaultProbes: BRDF LUT asset has unexpected size=%u expected=%u (%s).",
                         static_cast<unsigned>(outBytes.size()), static_cast<unsigned>(expected), absPath.c_str());
        outBytes.clear();
    }
    return false;
}

} // namespace

IblDefaultProbes::~IblDefaultProbes() = default;

void IblDefaultProbes::Ensure(rhi::IDevice* device) {
    if (!device)
        return;
    if (!m_envCubemap) {
        std::vector<uint8_t> cube;
        ibl_cpu::BuildNeutralSkylightCubemapRgba16F(cube, 64u);
        rhi::TextureDesc dc{};
        dc.type = rhi::TextureType::TextureCube;
        dc.width = 64u;
        dc.height = 64u;
        dc.depthOrArrayLayers = 6u;
        dc.format = rhi::RHITextureFormat::RGBA16F;
        dc.usage = rhi::TextureUsage::ShaderResource;
        dc.mipLevels = 1u;
        dc.debugName = "EyelibRendering.Lighting.IblEnvCubemap";
        m_envCubemap = device->CreateTexture(dc, cube.empty() ? nullptr : cube.data());
        if (!m_envCubemap) {
            static int s_warn = 0;
            if (s_warn < 4) {
                ++s_warn;
                core::Log::Warn("IblDefaultProbes: failed to create default environment cubemap.");
            }
        }
    }
    if (!m_brdfLut) {
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<uint8_t> lut;
        bool fromAsset = TryLoadBrdfLutRawBytes(lut);
        bool fromGpuFallback = false;
        if (!fromAsset) {
            static bool s_missingAssetWarned = false;
            if (!s_missingAssetWarned) {
                s_missingAssetWarned = true;
                core::Log::Warn(
                    "IblDefaultProbes: BRDF LUT asset not found; trying GPU fallback first, then bounded CPU fallback. "
                    "Expected one of: assets/textures/rendering/ibl/brdf_lut_rg16f_256x256.bin "
                    "or assets/textures/ibl/brdf_lut_rg16f_256x256.bin");
            }
            if (m_envCubemap) {
                m_gpuFallbackIbl = std::make_unique<IBL>();
                m_gpuFallbackIbl->SetDevice(device);
                if (m_gpuFallbackIbl->Initialize(m_envCubemap, 16u, 64u)) {
                    rhi::ITexture* lutTex = m_gpuFallbackIbl->GetBRDFLUTTextureRHI();
                    if (lutTex) {
                        m_brdfLut = lutTex;
                        m_brdfOwnedByIbl = true;
                        fromGpuFallback = true;
                    }
                }
                if (!fromGpuFallback)
                    m_gpuFallbackIbl.reset();
            }
            if (!fromGpuFallback)
                ibl_cpu::BuildBrdfLutRgba16F(lut, kFallbackBrdfLutDim, kFallbackBrdfLutSamples);
        }
        if (!m_brdfLut) {
            rhi::TextureDesc d{};
            d.type = rhi::TextureType::Texture2D;
            d.width = kFallbackBrdfLutDim;
            d.height = kFallbackBrdfLutDim;
            d.format = rhi::RHITextureFormat::RGBA16F;
            d.usage = rhi::TextureUsage::ShaderResource;
            d.mipLevels = 1u;
            d.debugName = "EyelibRendering.Lighting.IblBrdfLut";
            m_brdfLut = device->CreateTexture(d, lut.empty() ? nullptr : lut.data());
        }
        const auto t1 = std::chrono::steady_clock::now();
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        if (!m_brdfLut) {
            static int s_warn = 0;
            if (s_warn < 4) {
                ++s_warn;
                core::Log::Warn("IblDefaultProbes: failed to create BRDF LUT texture.");
            }
        } else {
            const char* pathKind = fromAsset ? "asset"
                                  : (fromGpuFallback ? "GPU fallback (IBL pass)" : "bounded CPU fallback");
            core::Log::InfoF("IblDefaultProbes: BRDF LUT ready (%s, %lld ms).", pathKind, ms);
        }
    }
}

void IblDefaultProbes::Shutdown(rhi::IDevice* device) noexcept {
    if (m_gpuFallbackIbl) {
        m_gpuFallbackIbl.reset();
        m_brdfLut = nullptr;
    }
    if (m_brdfLut && device && !m_brdfOwnedByIbl) {
        device->DestroyTexture(m_brdfLut);
        m_brdfLut = nullptr;
    }
    m_brdfOwnedByIbl = false;
    if (m_envCubemap && device) {
        device->DestroyTexture(m_envCubemap);
        m_envCubemap = nullptr;
    }
}

} // namespace rendering
} // namespace eyelib
