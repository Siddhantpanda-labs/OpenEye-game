#include "MaterialPreview.h"
#include "EditorContext.h"
#include "../EditorVulkanRenderSlice.h"

#include "api/asset/AssetRegistry.h"
#include "api/geometry/Primitives.h"
#include "api/render/CameraConventions.h"
#include "api/render/Material.h"
#include "api/render/Mesh.h"
#include "api/render/RenderPipeline.h"
#include "api/render/RenderScene.h"

#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

namespace openeye::editor {

namespace {

void* ResolvePreviewImGuiTexture(EditorContext* ctx, void* native) {
    if (!native) return nullptr;
    if (ctx && ctx->resolveImGuiTextureId) return ctx->resolveImGuiTextureId(native);
    return native;
}

} // namespace

void DrawMaterialEditorPreview(EditorContext* ctx, const ::eyelib::api::AssetId& matId) {
    if (!ctx || !ctx->materialPreviewPipeline || !ctx->assetRegistry || !matId.IsValid()) {
        if (ctx && !ctx->materialPreviewPipeline)
            ImGui::TextDisabled("Preview pipeline not initialized.");
        return;
    }

    ctx->materialPreviewPipeline->SetMode(eyelib::api::RenderPipeline::Mode::Forward);

    const eyelib::api::RenderDevice device = ctx->assetRegistry->GetRenderDevice();
    if (!device) {
        ImGui::TextDisabled("Preview unavailable (no render device).");
        return;
    }

    static std::shared_ptr<eyelib::api::Mesh> s_sphere;
    static void* s_deviceCookie = nullptr;
    void* devPtr = device.Get();
    if (!s_sphere || s_deviceCookie != devPtr) {
        s_deviceCookie = devPtr;
        s_sphere = eyelib::api::geometry::Primitives::CreateSphere(device, 0.5f, 48, 24);
    }
    if (!s_sphere) {
        ImGui::TextDisabled("Could not create preview mesh.");
        return;
    }

    std::shared_ptr<eyelib::api::Material> material = ctx->assetRegistry->GetMaterial(matId);
    if (!material) {
        ImGui::TextDisabled("Material not loaded for preview.");
        return;
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float maxSide = std::max(160.f, avail.x - 4.f);
    const float rawSide = std::min(avail.x - 4.f, 340.f);
    const float side = std::max(160.f, std::min(rawSide, maxSide));
    const uint32_t w = static_cast<uint32_t>(std::max(64.f, side));
    const uint32_t h = w;

    eyelib::api::RenderScene scene;
    eyelib::api::Drawable drawable;
    drawable.mesh = s_sphere;
    drawable.material = material;
    const float spin = static_cast<float>(ImGui::GetTime()) * 0.38f;
    drawable.transform = glm::rotate(eyelib::math::Mat4(1.f), spin, eyelib::math::Vec3(0.f, 1.f, 0.f));
    eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, nullptr);

    if (eyelib::api::DrawableRoutesToTransparentPass(drawable))
        scene.transparentStorage.push_back(std::move(drawable));
    else
        scene.opaqueStorage.push_back(std::move(drawable));

    scene.opaque = scene.opaqueStorage.empty() ? nullptr : scene.opaqueStorage.data();
    scene.opaqueCount = scene.opaqueStorage.size();
    scene.transparent = scene.transparentStorage.empty() ? nullptr : scene.transparentStorage.data();
    scene.transparentCount = scene.transparentStorage.size();

    const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.f;
    const float t = static_cast<float>(ImGui::GetTime()) * 0.22f;
    const eyelib::math::Vec3 eye(std::sin(t) * 2.35f, 0.42f, std::cos(t) * 2.35f);
    const eyelib::math::Vec3 target(0.f);
    const eyelib::math::Vec3 up(0.f, 1.f, 0.f);

    eyelib::api::CameraView cam{};
    cam.viewPos = eye;
    cam.view = eyelib::api::camera::BuildLookAtView(eye, target, up);
    cam.nearPlane = 0.05f;
    cam.farPlane = 80.f;
    cam.projection =
        eyelib::api::camera::BuildPerspectiveProjectionDegrees(36.f, aspect, cam.nearPlane, cam.farPlane);
    scene.camera = eyelib::api::camera::AdjustCameraForBackend(cam, ctx->materialPreviewPipeline->GetBackend());

    scene.light.direction = eyelib::math::Vec3(0.4f, -1.f, 0.28f);
    scene.light.color = eyelib::math::Vec3(1.f, 0.97f, 0.92f);
    scene.light.intensity = 2.6f;
    scene.light.ambient = eyelib::math::Vec3(0.11f, 0.11f, 0.13f);
    scene.light.exposure = 1.f;
    scene.light.castShadows = false;
    scene.skybox.reset();

    auto* rhi = static_cast<eyelib::render::rhi::IDevice*>(device.Get());
    VulkanRenderSliceScope vulkanSlice(rhi);
    ctx->materialPreviewPipeline->Resize(w, h);
    ctx->materialPreviewPipeline->Render(scene, nullptr);

    void* raw = ctx->materialPreviewPipeline->GetOutputColorTextureNativeHandle();
    void* tex = ResolvePreviewImGuiTexture(ctx, raw);
    if (tex && ctx->materialPreviewPipeline->HasValidOutput()) {
        ImGui::Image(tex, ImVec2(static_cast<float>(w), static_cast<float>(h)), ImVec2(0, 1), ImVec2(1, 0));
    } else {
        ImGui::TextDisabled("No preview output.");
    }
}

} // namespace openeye::editor
