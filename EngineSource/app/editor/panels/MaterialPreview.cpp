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
#include "api/rendering/RenderPipeline.h"
#include "api/rendering/RenderScene.h"
#include "api/rendering/RenderSliceScope.h"

#include "eyelib/math/Types.h"
#include "eyelib/math/TransformUtils.h"
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
    if (!ctx || !matId.IsValid()) return;

    // M3.1 cold-path branch: when the new-API material preview pipeline is wired (M3.9
    // device flip), drive it instead of the legacy one. Mesh / material handles route
    // through the new GPU cache (M3.7); until that lands the rendered sphere will be a
    // placeholder (zero tokens — façade tolerates this without crashing). Camera + light
    // values are inlined here using `eyelib::math` directly so this branch never includes
    // legacy `api/render/CameraConventions.h`.
    if (ctx->materialPreviewPipelineNew) {
        const ImVec2 availNew = ImGui::GetContentRegionAvail();
        const float sideNew = std::max(160.f, std::min(availNew.x - 4.f, 340.f));
        const uint32_t wn = static_cast<uint32_t>(std::max(64.f, sideNew));
        const uint32_t hn = wn;

        ctx->materialPreviewPipelineNew->SetMode(eyelib::api::rendering::RenderPipeline::Mode::Forward);

        eyelib::api::rendering::RenderScene sceneNew;
        const float aspectN = (hn > 0) ? static_cast<float>(wn) / static_cast<float>(hn) : 1.f;
        const float tn = static_cast<float>(ImGui::GetTime()) * 0.22f;
        const eyelib::math::Vec3 eyeN(std::sin(tn) * 2.35f, 0.42f, std::cos(tn) * 2.35f);
        const eyelib::math::Vec3 targetN(0.f);
        const eyelib::math::Vec3 upN(0.f, 1.f, 0.f);

        sceneNew.camera.viewPos = eyeN;
        sceneNew.camera.view = eyelib::math::LookAt(eyeN, targetN, upN);
        sceneNew.camera.nearPlane = 0.05f;
        sceneNew.camera.farPlane = 80.f;
        sceneNew.camera.projection =
            eyelib::math::Perspective(36.f * 3.14159265358979323846f / 180.0f, aspectN,
                                      sceneNew.camera.nearPlane, sceneNew.camera.farPlane);
        // Vulkan clip-space Y flip (matches new module's backend policy).
        sceneNew.camera.projection[1][1] = -sceneNew.camera.projection[1][1];

        // Placeholder drawable: the inspector preview becomes meaningful only once M3.7
        // resolves the inspected `matId` to a real `api::rendering::MaterialInstance` and
        // the sphere mesh to a real `api::rendering::Mesh` token.
        eyelib::api::rendering::DrawableInstance instanceN{};
        const float spinN = static_cast<float>(ImGui::GetTime()) * 0.38f;
        instanceN.worldTransform = eyelib::math::Compose(
            eyelib::math::Vec3(0.f),
            eyelib::math::Quat(eyelib::math::Vec3(0.f, spinN, 0.f)),
            eyelib::math::Vec3(1.f));
        instanceN.layer = eyelib::api::rendering::DrawLayer::Opaque;
        sceneNew.opaque.push_back(instanceN);

        eyelib::api::rendering::RenderSliceScope sliceN(ctx->materialPreviewPipelineNew->GetDevice());
        ctx->materialPreviewPipelineNew->Resize(wn, hn);
        ctx->materialPreviewPipelineNew->Render(sceneNew);

        void* rawN = ctx->materialPreviewPipelineNew->GetOutputColorTextureNativeHandle();
        void* texN = ResolvePreviewImGuiTexture(ctx, rawN);
        if (texN && ctx->materialPreviewPipelineNew->HasValidOutput()) {
            ImGui::Image(texN, ImVec2(static_cast<float>(wn), static_cast<float>(hn)), ImVec2(0, 1), ImVec2(1, 0));
        } else {
            ImGui::TextDisabled("No preview output (api/rendering).");
        }
        return;
    }

    if (!ctx->materialPreviewPipeline || !ctx->assetRegistry) {
        if (!ctx->materialPreviewPipeline)
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
