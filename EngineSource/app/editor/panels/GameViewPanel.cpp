#include "GameViewPanel.h"
#include "../EditorVulkanRenderSlice.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "EditorSceneBuilderNew.h"
#include "SkyboxCacheNew.h"
#include "GameRuntime.h"

#include "api/math/TransformUtils.h"
#include "api/render/CameraConventions.h"
#include "api/render/RenderPipeline.h"  // M4.8 — explicit include after GameRuntime.h diet (was transitive via api/render/All.h).
#include "api/render/RenderScene.h"
#include "api/rendering/RenderPipeline.h"
#include "api/rendering/RenderScene.h"
#include "api/rendering/RenderSliceScope.h"
#include "api/rendering/Skybox.h"
#include "api/world/All.h"
#include "api/debug/Profiler.h"
#include "eyelib/math/TransformUtils.h"
#include "eyelib/math/Types.h"
#include "eyelib/vr/XRViewHelpers.h"
#include "eyelib/world/SkyboxAssetId.h"

#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace openeye::editor::panels {

namespace {

using EntityId = ::eyelib::world::EntityId;

/** Build CameraView from world transform + Camera component (for debug preview). */
void SceneCameraToView(const ::eyelib::world::components::Transform& transform,
                       const ::eyelib::world::components::Camera& cam,
                       uint32_t viewportWidth, uint32_t viewportHeight,
                       eyelib::api::CameraView& out) {
    const float aspect = (viewportHeight > 0) ? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight) : (16.f / 9.f);
    out.nearPlane = cam.nearPlane;
    out.farPlane = cam.farPlane;
    out.projection = eyelib::api::camera::BuildPerspectiveProjectionDegrees(
        cam.fov, aspect, out.nearPlane, out.farPlane);
    out.view = eyelib::api::camera::BuildViewFromWorldTransform(
        transform.position, transform.rotation, transform.scale);
    out.viewPos = transform.position;
}

std::string GetEntityDisplayName(eyelib::api::world::World& world, EntityId id) {
    std::string name;
    if (world.GetEntityName(id, name) && !name.empty()) return name;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Camera %u", id.index);
    return buf;
}

void* ResolveGamePanelTextureId(EditorContext* ctx, void* nativeTexture) {
    if (!nativeTexture) return nullptr;
    if (ctx && ctx->resolveImGuiTextureId)
        return ctx->resolveImGuiTextureId(nativeTexture);
    return nativeTexture;
}

} // namespace

void ShowGameViewPanel(EditorContext* ctx) {
    if (!ctx) return;

    const char* title = "Game";
    ImGui::Begin(title);

    if (ctx->isPlaying && ctx->gameRuntime && ctx->gameRuntime->IsReady()) {
        ImVec2 region = ImGui::GetContentRegionAvail();
        uint32_t w = (region.x > 1.f) ? static_cast<uint32_t>(region.x) : 1u;
        uint32_t h = (region.y > 1.f) ? static_cast<uint32_t>(region.y) : 1u;
        if (auto* pipeNew = ctx->gameRuntime->GetRenderPipelineNew()) {
            eyelib::api::rendering::RenderSliceScope slice(pipeNew->GetDevice());
            (void)ctx->gameRuntime->Render(w, h);
        } else {
            eyelib::render::rhi::IDevice* rhiDev =
                static_cast<eyelib::render::rhi::IDevice*>(ctx->gameRuntime->GetDevice().Get());
            ::openeye::editor::VulkanRenderSliceScope vulkanSlice(rhiDev);
            (void)ctx->gameRuntime->Render(w, h);
        }
        float deltaSeconds = ImGui::GetIO().DeltaTime;
        if (deltaSeconds <= 0.f) deltaSeconds = 1.f / 60.f;
        ctx->gameRuntime->Tick(deltaSeconds);

        void* texId = ResolveGamePanelTextureId(ctx, ctx->gameRuntime->GetOutputColorTextureNativeHandle());
        if (texId) {
            ImVec2 size(static_cast<float>(w), static_cast<float>(h));
            ImGui::Image(texId, size, ImVec2(0, 0), ImVec2(1, 1));
        } else {
            ImGui::TextUnformatted("No output");
        }
    } else {
        // Debug: camera dropdown to preview a scene camera when not playing
        if (ImGui::Button("Camera")) {
            ImGui::OpenPopup("GameViewCamera");
        }
        if (ImGui::BeginPopup("GameViewCamera", ImGuiWindowFlags_AlwaysAutoResize)) {
            ctx->world.FlushTransforms();
            eyelib::api::world::World::RenderSnapshot snapshot;
            ctx->world.CaptureRenderSnapshot(snapshot);

            if (snapshot.cameras.empty() && snapshot.xrCameras.empty()) {
                ImGui::TextUnformatted("No cameras in scene.");
            } else {
                for (const auto& entry : snapshot.cameras) {
                    const EntityId e = entry.entity;
                    const bool isSelected = (ctx->gameViewDebugCamera.index == e.index && ctx->gameViewDebugCamera.generation == e.generation);
                    std::string label = GetEntityDisplayName(ctx->world, e);
                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        ctx->gameViewDebugCamera = isSelected ? ::eyelib::world::InvalidEntity : e;
                    }
                }
                for (const auto& entry : snapshot.xrCameras) {
                    const EntityId e = entry.entity;
                    const bool isSelected = (ctx->gameViewDebugCamera.index == e.index && ctx->gameViewDebugCamera.generation == e.generation);
                    std::string label = "XR: " + GetEntityDisplayName(ctx->world, e);
                    if (ImGui::Selectable(label.c_str(), isSelected)) {
                        ctx->gameViewDebugCamera = isSelected ? ::eyelib::world::InvalidEntity : e;
                    }
                }
            }
            ImGui::EndPopup();
        }

        const bool hasDebugCamera = ctx->gameViewDebugCamera != ::eyelib::world::InvalidEntity &&
                                   ctx->world.IsAlive(ctx->gameViewDebugCamera);
        // M3.5 cold-path: prefer `gameViewPipelineNew` when set (M3.9 device flip). The new
        // façade stubs `RenderStereo` (M1.8 XR pause), so non-playing XR camera preview falls
        // back to a one-line "not yet supported" message on the new path.
        if (ctx->gameViewPipelineNew && hasDebugCamera) {
            ImVec2 region = ImGui::GetContentRegionAvail();
            const uint32_t w = (region.x > 1.f) ? static_cast<uint32_t>(region.x) : 1u;
            const uint32_t h = (region.y > 1.f) ? static_cast<uint32_t>(region.y) : 1u;

            ctx->world.FlushTransforms();
            eyelib::api::world::World::RenderSnapshot snapshot;
            ctx->world.CaptureRenderSnapshot(snapshot);

            ::eyelib::world::components::Transform camTransform;
            ::eyelib::world::components::Camera camComp;
            const bool isRegularCamera =
                ctx->world.GetWorldTransform(ctx->gameViewDebugCamera, camTransform) &&
                ctx->world.GetCamera(ctx->gameViewDebugCamera, camComp);

            if (!isRegularCamera) {
                ImGui::TextDisabled("XR debug preview unsupported on api/rendering (stereo paused).");
            } else {
                static EditorSceneBuilderNew s_builderNew;
                eyelib::api::rendering::RenderScene sceneNew;
                eyelib::api::debug::Profiler::BeginSample("GameView Build (new)");
                s_builderNew.Build(sceneNew, snapshot, ctx->editorCamera, w, h, ctx->assetRoots,
                    ctx->assetRegistry, nullptr, nullptr,
                    ctx->gpuResolverNew, ctx->builtinManagerNew);
                // Match SceneViewPanel: attach the world's resolved skybox so the new
                // `SkyboxFeature` overlays the procedural / cubemap sky onto the game view.
                if (ctx->skyboxCacheNew) {
                    sceneNew.skybox = ctx->skyboxCacheNew->GetOrLoad(
                        ctx->gameViewPipelineNew->GetDevice(),
                        snapshot.skyboxAssetId, ctx->assetRoots,
                        ctx->assetRegistry, eyelib::world::skybox::kDefaultCrossFaceSize);
                }
                eyelib::api::debug::Profiler::EndSample();

                // Override editor-camera view with the scene-camera transform + projection,
                // matching the legacy SceneCameraToView() behavior. Vulkan clip-Y flip baked
                // in here (no legacy `api/render/CameraConventions.h` include in this branch).
                const float aspect = (h > 0u) ? static_cast<float>(w) / static_cast<float>(h) : (16.f / 9.f);
                const float fovRad = camComp.fov * 3.14159265358979323846f / 180.0f;
                sceneNew.camera.nearPlane = camComp.nearPlane;
                sceneNew.camera.farPlane = camComp.farPlane;
                sceneNew.camera.projection = eyelib::math::Perspective(fovRad, aspect,
                    sceneNew.camera.nearPlane, sceneNew.camera.farPlane);
                const eyelib::math::Mat4 worldM = eyelib::math::Compose(
                    camTransform.position, camTransform.rotation, camTransform.scale);
                sceneNew.camera.view = eyelib::math::Inverse(worldM);
                sceneNew.camera.viewPos = camTransform.position;

                eyelib::api::rendering::RenderSliceScope sliceN(ctx->gameViewPipelineNew->GetDevice());
                ctx->gameViewPipelineNew->Resize(w, h);
                eyelib::api::debug::Profiler::BeginSample("GameView Render (new)");
                ctx->gameViewPipelineNew->Render(sceneNew);
                eyelib::api::debug::Profiler::EndSample();

                void* texId = ResolveGamePanelTextureId(ctx, ctx->gameViewPipelineNew->GetOutputColorTextureNativeHandle());
                if (texId && ctx->gameViewPipelineNew->HasValidOutput()) {
                    ImGui::Image(texId, ImVec2(static_cast<float>(w), static_cast<float>(h)), ImVec2(0, 1), ImVec2(1, 0));
                } else {
                    ImGui::TextUnformatted("No preview output (api/rendering)");
                }
            }
            ImGui::End();
            return;
        }

        const bool canPreview = !ctx->isPlaying && hasDebugCamera && ctx->gameViewPipeline && ctx->assetRegistry;

        if (canPreview) {
            eyelib::render::rhi::IDevice* rhiDev =
                static_cast<eyelib::render::rhi::IDevice*>(ctx->gameViewPipeline->GetDevice().Get());
            ::openeye::editor::VulkanRenderSliceScope vulkanSlice(rhiDev);
            ImVec2 region = ImGui::GetContentRegionAvail();
            const uint32_t w = (region.x > 1.f) ? static_cast<uint32_t>(region.x) : 1u;
            const uint32_t h = (region.y > 1.f) ? static_cast<uint32_t>(region.y) : 1u;

            ctx->world.FlushTransforms();
            eyelib::api::world::World::RenderSnapshot snapshot;
            ctx->world.CaptureRenderSnapshot(snapshot);

            ::eyelib::world::components::Transform camTransform;
            ::eyelib::world::components::Camera camComp;
            ::eyelib::world::components::XRCamera xrCamComp;
            const bool isRegularCamera = ctx->world.GetWorldTransform(ctx->gameViewDebugCamera, camTransform) &&
                                         ctx->world.GetCamera(ctx->gameViewDebugCamera, camComp);
            const bool isXRCamera = !isRegularCamera &&
                                   ctx->world.GetWorldTransform(ctx->gameViewDebugCamera, camTransform) &&
                                   ctx->world.GetXRCamera(ctx->gameViewDebugCamera, xrCamComp);

            if (isRegularCamera) {
                static EditorSceneBuilder s_builder;
                eyelib::api::RenderScene scene;
                eyelib::api::debug::Profiler::BeginSample("GameView Build");
                s_builder.Build(scene, snapshot, ctx->editorCamera, w, h, ctx->assetRoots, ctx->assetRegistry, ctx->defaultMaterial, &ctx->world, nullptr, nullptr);
                eyelib::api::debug::Profiler::EndSample();
                SceneCameraToView(camTransform, camComp, w, h, scene.camera);

                ctx->gameViewPipeline->Resize(w, h);
                eyelib::api::debug::Profiler::BeginSample("GameView Render");
                ctx->gameViewPipeline->Render(scene, nullptr);
                eyelib::api::debug::Profiler::EndSample();

                void* texId = ResolveGamePanelTextureId(ctx, ctx->gameViewPipeline->GetOutputColorTextureNativeHandle());
                if (texId && ctx->gameViewPipeline->HasValidOutput()) {
                    ImGui::Image(texId, ImVec2(static_cast<float>(w), static_cast<float>(h)), ImVec2(0, 1), ImVec2(1, 0));
                } else {
                    ImGui::TextUnformatted("No preview output");
                }
            } else if (isXRCamera) {
                // Single-pass stereo: one render, two viewports (left | right)
                static EditorSceneBuilder s_builder;
                eyelib::api::RenderScene scene;
                eyelib::api::debug::Profiler::BeginSample("GameView Build");
                s_builder.Build(scene, snapshot, ctx->editorCamera, w, h, ctx->assetRoots, ctx->assetRegistry, ctx->defaultMaterial, &ctx->world, nullptr, nullptr);
                eyelib::api::debug::Profiler::EndSample();

                eyelib::vr::XRViewData leftData, rightData;
                eyelib::vr::StereoViewDataFromCenterPose(camTransform.position, camTransform.rotation,
                    eyelib::vr::kDefaultStereoIPD, eyelib::vr::kDefaultStereoFovHalfX, eyelib::vr::kDefaultStereoFovHalfY,
                    leftData, rightData);

                const float nearPlane = xrCamComp.nearPlane;
                const float farPlane = xrCamComp.farPlane;
                eyelib::vr::XRViewProjection leftProj = eyelib::vr::ViewProjectionFromXRView(leftData, nearPlane, farPlane);
                eyelib::vr::XRViewProjection rightProj = eyelib::vr::ViewProjectionFromXRView(rightData, nearPlane, farPlane);

                eyelib::api::CameraView leftCam, rightCam;
                leftCam.view = leftProj.view;
                leftCam.projection = leftProj.projection;
                leftCam.viewPos = leftProj.viewPos;
                leftCam.nearPlane = leftProj.nearPlane;
                leftCam.farPlane = leftProj.farPlane;
                rightCam.view = rightProj.view;
                rightCam.projection = rightProj.projection;
                rightCam.viewPos = rightProj.viewPos;
                rightCam.nearPlane = rightProj.nearPlane;
                rightCam.farPlane = rightProj.farPlane;

                ctx->gameViewPipeline->Resize(w, h);
                eyelib::api::debug::Profiler::BeginSample("GameView Render");
                ctx->gameViewPipeline->RenderStereo(scene, leftCam, rightCam);
                eyelib::api::debug::Profiler::EndSample();

                void* texId = ResolveGamePanelTextureId(ctx, ctx->gameViewPipeline->GetOutputColorTextureNativeHandle());
                if (texId && ctx->gameViewPipeline->HasValidOutput()) {
                    ImGui::Image(texId, ImVec2(static_cast<float>(w), static_cast<float>(h)), ImVec2(0, 1), ImVec2(1, 0));
                } else {
                    ImGui::TextUnformatted("No stereo preview output");
                }
            } else {
                ImGui::TextUnformatted("Camera entity missing transform or component.");
            }
        } else {
            if (!hasDebugCamera || !ctx->gameViewPipeline) {
                ImGui::TextUnformatted("Press Play to run the game, or select a Camera above to preview.");
            }
        }
    }

    ImGui::End();
}

} // namespace openeye::editor::panels
