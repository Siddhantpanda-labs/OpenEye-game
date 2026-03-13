#include "GameViewPanel.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "GameRuntime.h"

#include "api/math/TransformUtils.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"
#include "api/debug/Profiler.h"
#include "eyelib/vr/XRViewHelpers.h"

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
    const float fovRad = cam.fov * (3.14159265f / 180.f);
    out.projection = eyelib::math::Perspective(fovRad, aspect, out.nearPlane, out.farPlane);

    eyelib::math::Mat4 world = eyelib::math::Compose(transform.position, transform.rotation, transform.scale);
    out.view = eyelib::math::Inverse(world);
    out.viewPos = transform.position;
}

std::string GetEntityDisplayName(eyelib::api::world::World& world, EntityId id) {
    std::string name;
    if (world.GetEntityName(id, name) && !name.empty()) return name;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Camera %u", id.index);
    return buf;
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
        // Render before Tick so the first frame shows bodies at their initial position (no jump on Play).
        (void)ctx->gameRuntime->Render(w, h);
        float deltaSeconds = ImGui::GetIO().DeltaTime;
        if (deltaSeconds <= 0.f) deltaSeconds = 1.f / 60.f;
        ctx->gameRuntime->Tick(deltaSeconds);

        void* texId = ctx->gameRuntime->GetOutputColorTextureNativeHandle();
        if (texId) {
            ImVec2 size(static_cast<float>(w), static_cast<float>(h));
            ImGui::Image(texId, size, ImVec2(0, 1), ImVec2(1, 0));
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
        const bool canPreview = !ctx->isPlaying && hasDebugCamera && ctx->gameViewPipeline && ctx->assetRegistry;

        if (canPreview) {
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
                s_builder.Build(scene, snapshot, ctx->editorCamera, w, h, ctx->assetRoots, ctx->assetRegistry, ctx->defaultMaterial, &ctx->world, nullptr);
                eyelib::api::debug::Profiler::EndSample();
                SceneCameraToView(camTransform, camComp, w, h, scene.camera);

                ctx->gameViewPipeline->Resize(w, h);
                eyelib::api::debug::Profiler::BeginSample("GameView Render");
                ctx->gameViewPipeline->Render(scene, nullptr);
                eyelib::api::debug::Profiler::EndSample();

                void* texId = ctx->gameViewPipeline->GetOutputColorTextureNativeHandle();
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
                s_builder.Build(scene, snapshot, ctx->editorCamera, w, h, ctx->assetRoots, ctx->assetRegistry, ctx->defaultMaterial, &ctx->world, nullptr);
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

                void* texId = ctx->gameViewPipeline->GetOutputColorTextureNativeHandle();
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
