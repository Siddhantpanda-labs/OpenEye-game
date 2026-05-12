#include "api/rendering/Skybox.h"

#include "api/core/Log.h"
#include "api/rendering/internal/ResourceAccess.h"

#include "eyelib/rendering/resources/TextureCube.h"
#include "eyelib/rendering/rhi/RHI.h"

#include <utility>

namespace eyelib {
namespace api {
namespace rendering {

class Skybox::Impl {
public:
    Skybox::Kind kind = Skybox::Kind::None;
    // Owned only when `kind == Cubemap`; null otherwise. The internal class manages the
    // RHI texture lifetime, so destroying `cube` frees the GPU resource automatically.
    std::unique_ptr<eyelib::rendering::TextureCube> cube;
};

namespace {

eyelib::rendering::rhi::IDevice* AsInternalDevice(RenderDevice device) noexcept {
    return static_cast<eyelib::rendering::rhi::IDevice*>(device.Get());
}

}  // namespace

Skybox::Skybox() : m_impl(std::make_unique<Impl>()) {}

Skybox::~Skybox() = default;

Skybox::Skybox(Skybox&& other) noexcept = default;
Skybox& Skybox::operator=(Skybox&& other) noexcept = default;

bool Skybox::LoadCrossLayout(RenderDevice device, const std::string& path, uint32_t faceSize,
                             bool isSRGB, bool generateMipmaps) {
    if (!m_impl) return false;
    auto* internalDevice = AsInternalDevice(device);
    if (!internalDevice) {
        Log::Warn("Skybox::LoadCrossLayout: device is null.");
        return false;
    }
    auto cube = std::make_unique<eyelib::rendering::TextureCube>();
    if (!cube->LoadFromCrossLayout(internalDevice, path, faceSize, isSRGB, generateMipmaps)) {
        Log::WarnF("Skybox::LoadCrossLayout: failed to load '%s' (faceSize=%u).", path.c_str(), faceSize);
        return false;
    }
    m_impl->cube = std::move(cube);
    m_impl->kind = Skybox::Kind::Cubemap;
    return true;
}

bool Skybox::LoadSixFaces(RenderDevice device, const std::array<std::string, 6>& faces,
                          bool isSRGB, bool generateMipmaps) {
    if (!m_impl) return false;
    auto* internalDevice = AsInternalDevice(device);
    if (!internalDevice) {
        Log::Warn("Skybox::LoadSixFaces: device is null.");
        return false;
    }
    auto cube = std::make_unique<eyelib::rendering::TextureCube>();
    if (!cube->LoadFromFiles(internalDevice, faces[0], faces[1], faces[2], faces[3], faces[4], faces[5],
                             isSRGB, generateMipmaps)) {
        Log::Warn("Skybox::LoadSixFaces: TextureCube::LoadFromFiles failed.");
        return false;
    }
    m_impl->cube = std::move(cube);
    m_impl->kind = Skybox::Kind::Cubemap;
    return true;
}

void Skybox::SetProcedural() noexcept {
    if (!m_impl) return;
    m_impl->cube.reset();
    m_impl->kind = Skybox::Kind::Procedural;
}

void Skybox::Reset() noexcept {
    if (!m_impl) return;
    m_impl->cube.reset();
    m_impl->kind = Skybox::Kind::None;
}

Skybox::Kind Skybox::GetKind() const noexcept {
    return m_impl ? m_impl->kind : Skybox::Kind::None;
}

int Skybox::GetSize() const noexcept {
    if (!m_impl || !m_impl->cube) return 0;
    return m_impl->cube->GetSize();
}

bool Skybox::IsCubemapValid() const noexcept {
    if (!m_impl) return false;
    return m_impl->kind == Skybox::Kind::Cubemap && m_impl->cube && m_impl->cube->IsValid();
}

// ---- SkyboxAccess (friend-only seam for RenderPipeline.cpp) -------------------------

SkyboxAccess::Request SkyboxAccess::Resolve(const Skybox& publicSkybox) noexcept {
    Request out{};
    if (!publicSkybox.m_impl) return out;
    out.kind = publicSkybox.m_impl->kind;
    if (out.kind == Skybox::Kind::Cubemap && publicSkybox.m_impl->cube) {
        out.cubemap = publicSkybox.m_impl->cube->GetTextureRHI();
        // Drop back to None semantics if the GPU upload silently failed (loader returned
        // false but caller built a Skybox externally). RenderPipeline.cpp checks this.
        if (!out.cubemap || !out.cubemap->IsGpuBindable()) {
            out.kind = Skybox::Kind::None;
            out.cubemap = nullptr;
        }
    }
    return out;
}

}  // namespace rendering
}  // namespace api
}  // namespace eyelib
