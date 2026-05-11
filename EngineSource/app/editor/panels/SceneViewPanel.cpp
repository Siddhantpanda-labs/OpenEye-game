#include "SceneViewPanel.h"
#include "../EditorVulkanRenderSlice.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "EditorSceneBuilderNew.h"
#include "ui/LightGizmoIcons.h"
#include "PanelCommon.h"

#include "api/core/Log.h"
#include "api/render/RenderPipeline.h"
#include "api/render/RenderScene.h"
#include "api/render/CameraConventions.h"
#include "api/rendering/RenderPipeline.h"
#include "api/rendering/RenderScene.h"
#include "api/rendering/RenderSliceScope.h"
#include "api/world/All.h"
#include "api/math/TransformUtils.h"
#include "api/debug/Profiler.h"
#include "eyelib/render/rhi/RHI.h"

#include "ImGuizmo.h"

#include <imgui.h>
#include <cmath>
#include <cstring>
#include <optional>

namespace openeye::editor::panels {

namespace {
const float kPitchMin = -1.55f;  // just below -pi/2 to avoid flip
const float kPitchMax =  1.55f;

using EntityId = ::eyelib::world::EntityId;
using Vec3 = eyelib::math::Vec3;
using Vec4 = eyelib::math::Vec4;
using Mat4 = eyelib::math::Mat4;
using Quat = eyelib::math::Quat;

/** Colored line: (segment, RGBA). For SetPendingDebugLinesColored. */
using ColoredLine = std::pair<std::pair<Vec3, Vec3>, Vec4>;
using ColoredLines = std::vector<ColoredLine>;

constexpr uint32_t kEntityIdIndexBits = 20u;
constexpr uint32_t kEntityIdIndexMask = (1u << kEntityIdIndexBits) - 1u;
constexpr uint32_t kEntityIdGenBits = 12u;

inline uint32_t PackEntityId(EntityId e) {
    return (e.index & kEntityIdIndexMask) | ((e.generation & ((1u << kEntityIdGenBits) - 1u)) << kEntityIdIndexBits);
}
inline EntityId UnpackEntityId(uint32_t id) {
    if (id == 0u) return ::eyelib::world::InvalidEntity;
    EntityId e;
    e.index = id & kEntityIdIndexMask;
    e.generation = (id >> kEntityIdIndexBits) & ((1u << kEntityIdGenBits) - 1u);
    return e;
}

void BuildViewProjection(const EditorCamera& cam, float viewportW, float viewportH, ::eyelib::render::RenderBackend backend,
                         Mat4& outView, Mat4& outProj) {
    eyelib::api::camera::FreeFlyCameraDesc cameraDesc;
    cameraDesc.position = Vec3(cam.position[0], cam.position[1], cam.position[2]);
    cameraDesc.yaw = cam.yaw;
    cameraDesc.pitch = cam.pitch;
    cameraDesc.fovDegrees = cam.fovDegrees;
    cameraDesc.nearPlane = 0.1f;
    cameraDesc.farPlane = 1000.0f;

    const auto baseCamera = eyelib::api::camera::BuildFreeFlyCameraView(cameraDesc, viewportW, viewportH);
    const auto adjustedCamera = eyelib::api::camera::AdjustCameraForBackend(baseCamera, backend);
    outView = adjustedCamera.view;
    outProj = adjustedCamera.projection;
}

bool WorldToScreen(const Vec3& world, const Mat4& view, const Mat4& proj,
                   float viewportX, float viewportY, float viewportW, float viewportH,
                   float& outScreenX, float& outScreenY) {
    Vec4 clip = proj * view * Vec4(world.x, world.y, world.z, 1.f);
    if (clip.w <= 0.f) return false;
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f) return false;
    outScreenX = viewportX + (ndcX + 1.f) * 0.5f * viewportW;
    outScreenY = viewportY + (1.f - (ndcY + 1.f) * 0.5f) * viewportH;
    return true;
}

Vec3 DirectionFromTransform(const ::eyelib::world::components::Transform& t) {
    Mat4 m = eyelib::math::Compose(t.position, t.rotation, t.scale);
    Vec3 d(-m[2][0], -m[2][1], -m[2][2]);
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len > 1e-6f) {
        d.x /= len; d.y /= len; d.z /= len;
    } else {
        d = Vec3(0.f, -1.f, 0.f);
    }
    return d;
}

constexpr float kLightIconSize = 52.f;
constexpr float kDirectionArrowLength = 2.5f;
constexpr float kSpotArrowLength = 1.8f;
constexpr unsigned kOrangeColor = IM_COL32(255, 140, 0, 255);
constexpr float kArrowThickness = 2.5f;
constexpr unsigned kColliderCircleSegments = 24;

/** Snapped grid (XZ at y=0) centered on camera, and XYZ axes at origin. Appends colored lines to \a out. */
void CollectGridAndAxisLinesColored(const EditorCamera& cam, ColoredLines& out) {
    const float gridStep = 2.f;
    const float halfExtent = 50.f;
    const float axisLen = 4.f;

    // Snap camera XZ to grid so the grid "follows" the camera and new lines appear on the opposite side as you move
    const float camX = cam.position[0];
    const float camZ = cam.position[2];
    const float centerX = std::floor(camX / gridStep + 0.5f) * gridStep;
    const float centerZ = std::floor(camZ / gridStep + 0.5f) * gridStep;
    const float xMin = centerX - halfExtent;
    const float xMax = centerX + halfExtent;
    const float zMin = centerZ - halfExtent;
    const float zMax = centerZ + halfExtent;

    const Vec4 gridColor(0.f, 0.f, 0.f, 1.f);  // black
    const Vec4 axisXColor(1.f, 0.f, 0.f, 1.f); // red
    const Vec4 axisYColor(0.f, 1.f, 0.f, 1.f); // green
    const Vec4 axisZColor(0.f, 0.f, 1.f, 1.f); // blue

    // Grid: lines along X (varying Z) and along Z (varying X)
    for (float z = zMin; z <= zMax; z += gridStep) {
        out.push_back({ { Vec3(xMin, 0.f, z), Vec3(xMax, 0.f, z) }, gridColor });
    }
    for (float x = xMin; x <= xMax; x += gridStep) {
        out.push_back({ { Vec3(x, 0.f, zMin), Vec3(x, 0.f, zMax) }, gridColor });
    }
    // XYZ axes at origin
    out.push_back({ { Vec3(0.f, 0.f, 0.f), Vec3(axisLen, 0.f, 0.f) }, axisXColor });
    out.push_back({ { Vec3(0.f, 0.f, 0.f), Vec3(0.f, axisLen, 0.f) }, axisYColor });
    out.push_back({ { Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, axisLen) }, axisZColor });
}

Vec3 TransformPoint(const ::eyelib::world::components::Transform& t, const Vec3& local) {
    Vec3 scaled(t.scale.x * local.x, t.scale.y * local.y, t.scale.z * local.z);
    return Vec3(t.position.x, t.position.y, t.position.z) + (t.rotation * scaled);
}

void CollectColliderDebugLines(const eyelib::api::world::World::RenderSnapshot& snapshot,
    std::vector<std::pair<Vec3, Vec3>>& out) {
    using CT = ::eyelib::world::components::Collider::Type;
    for (const auto& entry : snapshot.colliders) {
        auto optTr = snapshot.GetTransform(entry.entity);
        if (!optTr) continue;
        const auto& tr = *optTr;
        const auto& col = entry.component;
        if (col.type == CT::Box) {
            float hx = col.size.x * 0.5f, hy = col.size.y * 0.5f, hz = col.size.z * 0.5f;
            Vec3 corners[8];
            for (int i = 0; i < 8; ++i) {
                float sx = (i & 1) ? hx : -hx, sy = (i & 2) ? hy : -hy, sz = (i & 4) ? hz : -hz;
                corners[i] = TransformPoint(tr, Vec3(col.center.x + sx, col.center.y + sy, col.center.z + sz));
            }
            const int edges[12][2] = {{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}};
            for (int e = 0; e < 12; ++e) out.push_back({corners[edges[e][0]], corners[edges[e][1]]});
        } else if (col.type == CT::Sphere) {
            float r = col.radius;
            for (int plane = 0; plane < 3; ++plane) {
                for (unsigned seg = 0; seg < kColliderCircleSegments; ++seg) {
                    float t0 = 2.f * 3.14159265f * static_cast<float>(seg) / static_cast<float>(kColliderCircleSegments);
                    float t1 = 2.f * 3.14159265f * static_cast<float>(seg + 1) / static_cast<float>(kColliderCircleSegments);
                    Vec3 local0, local1;
                    if (plane == 0) { local0 = Vec3(col.center.x, col.center.y + r * std::cos(t0), col.center.z + r * std::sin(t0)); local1 = Vec3(col.center.x, col.center.y + r * std::cos(t1), col.center.z + r * std::sin(t1)); }
                    else if (plane == 1) { local0 = Vec3(col.center.x + r * std::cos(t0), col.center.y, col.center.z + r * std::sin(t0)); local1 = Vec3(col.center.x + r * std::cos(t1), col.center.y, col.center.z + r * std::sin(t1)); }
                    else { local0 = Vec3(col.center.x + r * std::cos(t0), col.center.y + r * std::sin(t0), col.center.z); local1 = Vec3(col.center.x + r * std::cos(t1), col.center.y + r * std::sin(t1), col.center.z); }
                    out.push_back({TransformPoint(tr, local0), TransformPoint(tr, local1)});
                }
            }
        } else if (col.type == CT::Capsule) {
            float r = col.radius, halfH = col.height * 0.5f - col.radius;
            for (unsigned seg = 0; seg < kColliderCircleSegments; ++seg) {
                float t0 = 2.f * 3.14159265f * static_cast<float>(seg) / static_cast<float>(kColliderCircleSegments);
                float t1 = 2.f * 3.14159265f * static_cast<float>(seg + 1) / static_cast<float>(kColliderCircleSegments);
                float c0 = r * std::cos(t0), s0 = r * std::sin(t0), c1 = r * std::cos(t1), s1 = r * std::sin(t1);
                Vec3 l0(col.center.x + c0, col.center.y + halfH, col.center.z + s0), l1(col.center.x + c1, col.center.y + halfH, col.center.z + s1);
                Vec3 lb0(col.center.x + c0, col.center.y - halfH, col.center.z + s0), lb1(col.center.x + c1, col.center.y - halfH, col.center.z + s1);
                out.push_back({TransformPoint(tr, l0), TransformPoint(tr, l1)});
                out.push_back({TransformPoint(tr, lb0), TransformPoint(tr, lb1)});
            }
            for (unsigned k = 0; k < 4; ++k) {
                float t = 2.f * 3.14159265f * static_cast<float>(k) / 4.f;
                Vec3 l(col.center.x + r * std::cos(t), col.center.y + halfH, col.center.z + r * std::sin(t));
                Vec3 lb(col.center.x + r * std::cos(t), col.center.y - halfH, col.center.z + r * std::sin(t));
                out.push_back({TransformPoint(tr, l), TransformPoint(tr, lb)});
            }
        }
    }
}

} // namespace

void ShowSceneViewPanel(EditorContext* ctx) {
    if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_None)) {
        if (!ctx) {
            ImGui::TextUnformatted("(No context)");
            ImGui::End();
            return;
        }

        // Top-right: gizmo space switch (Local = entity axes, World = world axes)
        const float availX = ImGui::GetContentRegionAvail().x;
        const float gizmoSwitchWidth = 140.f;
        if (availX > gizmoSwitchWidth) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availX - gizmoSwitchWidth);
        }
        ImGui::PushID("scene_gizmo_space");
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Gizmo:");
        ImGui::SameLine();
        if (ImGui::Selectable("Local", ctx->sceneGizmoLocal, 0, ImVec2(50.f, 0.f))) ctx->sceneGizmoLocal = true;
        ImGui::SameLine();
        if (ImGui::Selectable("World", !ctx->sceneGizmoLocal, 0, ImVec2(50.f, 0.f))) ctx->sceneGizmoLocal = false;
        ImGui::PopID();

        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 64.0f) size.x = 64.0f;
        if (size.y < 64.0f) size.y = 64.0f;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        // No item over the viewport so ImGuizmo::Manipulate can activate (it requires !IsAnyItemHovered).
        // Space is reserved by SetCursorScreenPos below; viewport hover = mouse inside viewport rect.
        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const bool viewportHovered = (mousePos.x >= pos.x && mousePos.x < pos.x + size.x && mousePos.y >= pos.y && mousePos.y < pos.y + size.y);
        if (viewportHovered) {
            EditorCamera& cam = ctx->editorCamera;
            ImGuiIO& io = ImGui::GetIO();
            const float dt = io.DeltaTime > 0.f ? io.DeltaTime : 0.016f;

            // Right-drag: rotate camera (drag right = look right, drag up = look up)
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                cam.yaw   += io.MouseDelta.x * cam.mouseSensitivity;
                cam.pitch -= io.MouseDelta.y * cam.mouseSensitivity;
                if (cam.pitch > kPitchMax) cam.pitch = kPitchMax;
                if (cam.pitch < kPitchMin) cam.pitch = kPitchMin;
            }

            // WASD + Q/E: free-fly move (forward = -Z when yaw=0 pitch=0, match EditorSceneBuilder)
            const float cy = std::cos(cam.pitch);
            const float sy = std::sin(cam.pitch);
            const float ct = std::cos(cam.yaw);
            const float st = std::sin(cam.yaw);
            const float forwardX = cy * st;
            const float forwardY = sy;
            const float forwardZ = -cy * ct;
            float rx = cy * ct;
            float rz = cy * st;
            const float rlen = std::sqrt(rx * rx + rz * rz);
            if (rlen > 1e-5f) {
                rx /= rlen;
                rz /= rlen;
            }
            const float move = cam.moveSpeed * dt;
            if (ImGui::IsKeyDown(ImGuiKey_W)) { cam.position[0] += forwardX * move; cam.position[1] += forwardY * move; cam.position[2] += forwardZ * move; }
            if (ImGui::IsKeyDown(ImGuiKey_S)) { cam.position[0] -= forwardX * move; cam.position[1] -= forwardY * move; cam.position[2] -= forwardZ * move; }
            if (ImGui::IsKeyDown(ImGuiKey_A)) { cam.position[0] -= rx * move; cam.position[2] -= rz * move; }
            if (ImGui::IsKeyDown(ImGuiKey_D)) { cam.position[0] += rx * move; cam.position[2] += rz * move; }
            if (ImGui::IsKeyDown(ImGuiKey_Q)) { cam.position[1] -= move; }
            if (ImGui::IsKeyDown(ImGuiKey_E)) { cam.position[1] += move; }
        }

        const uint32_t w = static_cast<uint32_t>(size.x);
        const uint32_t h = static_cast<uint32_t>(size.y);
        // M2.4 — new-API render path. Cold during the M2/M3 dual-stack window per the
        // M2.1 device-strategy decision (`ctx->sceneViewPipelineNew` stays nullptr until
        // the M3 closeout flips ImGui-Vulkan + game view + material preview to the new
        // RHI device together). When it does become active, this branch takes over for
        // the scene-view render only; surrounding UI (camera input, light gizmos,
        // ImGuizmo, status bar) keeps reading from `ctx->world` directly and is
        // pipeline-agnostic. Picking + outline + debug lines are paused here per the
        // M2.5 / M2.6 decisions (façade safe-stubs already documented in M1.7).
        const bool useNewPipeline = (ctx->sceneViewPipelineNew != nullptr) && w > 0 && h > 0;
        if (useNewPipeline) {
            eyelib::api::world::World::RenderSnapshot newSnapshot;
            ctx->world.FlushTransforms();
            ctx->world.CaptureRenderSnapshot(newSnapshot);

            static EditorSceneBuilderNew s_newBuilder;
            eyelib::api::rendering::RenderScene newScene;
            eyelib::api::debug::Profiler::BeginSample("SceneView Build (new)");
            s_newBuilder.Build(newScene, newSnapshot, ctx->editorCamera, w, h, ctx->assetRoots,
                ctx->assetRegistry, nullptr, nullptr,
                ctx->gpuResolverNew, ctx->builtinManagerNew);
            eyelib::api::debug::Profiler::EndSample();

            // M3.6 — bracket new-pipeline recording in the api/rendering slice scope so
            // editor scene-view work submits on its own primary command buffer (before the
            // ImGui compose buffer), exactly mirroring the legacy editor's slicing model.
            eyelib::api::rendering::RenderSliceScope sliceN(ctx->sceneViewPipelineNew->GetDevice());
            ctx->sceneViewPipelineNew->Resize(w, h);

            // Editor debug-line overlay: grid + axes + collider helpers. Goes through the
            // public `SetPendingDebugLinesColored` parity surface; the new module's
            // `EditorFeature` consumes them on the next `Render` and clears (single-frame
            // contract). The view/proj args are ignored by the new pipeline (it uses
            // `RenderScene::camera` to drive `PerView`), but we pass them for legacy parity.
            {
                Mat4 viewN, projN;
                BuildViewProjection(ctx->editorCamera, size.x, size.y,
                    ::eyelib::render::RenderBackend::Vulkan, viewN, projN);
                ColoredLines coloredN;
                CollectGridAndAxisLinesColored(ctx->editorCamera, coloredN);
                if (!newSnapshot.colliders.empty()) {
                    std::vector<std::pair<Vec3, Vec3>> colliderLines;
                    colliderLines.reserve(newSnapshot.colliders.size() * 64u);
                    CollectColliderDebugLines(newSnapshot, colliderLines);
                    const Vec4 green(0.f, 1.f, 0.f, 1.f);
                    for (const auto& line : colliderLines)
                        coloredN.push_back({ line, green });
                }
                ctx->sceneViewPipelineNew->SetPendingDebugLinesColored(viewN, projN, coloredN);
            }

            eyelib::api::debug::Profiler::BeginSample("SceneView Render (new)");
            ctx->sceneViewPipelineNew->Render(newScene);
            eyelib::api::debug::Profiler::EndSample();

            void* rawTexId = ctx->sceneViewPipelineNew->GetOutputColorTextureNativeHandle();
            void* texId = rawTexId;
            if (ctx->resolveImGuiTextureId)
                texId = ctx->resolveImGuiTextureId(texId);
            if (texId && ctx->sceneViewPipelineNew->HasValidOutput()) {
                ImGui::GetWindowDrawList()->AddImage(texId, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    ImVec2(0, 0), ImVec2(1, 1));
            } else {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 42, 54, 255));
                const char* label = "Scene view (new pipeline, no output yet)";
                ImVec2 textSize = ImGui::CalcTextSize(label);
                draw->AddText(ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
                    IM_COL32(180, 180, 180, 255), label);
            }

            ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + ImGui::GetStyle().ItemSpacing.y));
            ImGui::Text("Drawables: %zu (api/rendering)", newScene.opaque.size() + newScene.transparent.size());
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                EditorCamera& cam = ctx->editorCamera;
                ImGui::DragFloat3("Position", cam.position, 0.02f);
                ImGui::DragFloat("Yaw", &cam.yaw, 0.01f);
                if (ImGui::DragFloat("Pitch", &cam.pitch, 0.01f, kPitchMin, kPitchMax)) {
                    if (cam.pitch > kPitchMax) cam.pitch = kPitchMax;
                    if (cam.pitch < kPitchMin) cam.pitch = kPitchMin;
                }
                ImGui::DragFloat("Move speed", &cam.moveSpeed, 0.1f, 0.5f, 50.0f);
                ImGui::DragFloat("Mouse sensitivity", &cam.mouseSensitivity, 0.0001f, 0.0005f, 0.02f, "%.4f");
            }

            // Gizmo in the new-pipeline branch (same behavior as legacy path).
            EntityId sel = ctx->selectedEntity;
            if (sel != ::eyelib::world::InvalidEntity && ctx->world.IsAlive(sel)) {
                std::optional<::eyelib::world::components::Transform> worldOpt = newSnapshot.GetTransform(sel);
                if (!worldOpt) {
                    ::eyelib::world::components::Transform worldTrTemp;
                    if (ctx->world.GetWorldTransform(sel, worldTrTemp))
                        worldOpt = worldTrTemp;
                }
                if (worldOpt) {
                    Mat4 view, proj;
                    BuildViewProjection(ctx->editorCamera, size.x, size.y, ::eyelib::render::RenderBackend::Vulkan, view, proj);
                    Mat4 model = eyelib::math::Compose(worldOpt->position, worldOpt->rotation, worldOpt->scale);
                    float modelMatrix[16];
                    std::memcpy(modelMatrix, &model[0][0], sizeof(modelMatrix));

                    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                    ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);
                    const ImGuizmo::OPERATION op = ImGuizmo::OPERATION::UNIVERSAL;
                    const ImGuizmo::MODE mode = ctx->sceneGizmoLocal ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD;
                    if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0], op, mode, modelMatrix)) {
                        Vec3 posOut, scaleOut;
                        Quat rotOut;
                        eyelib::math::Decompose(*reinterpret_cast<Mat4*>(modelMatrix), posOut, rotOut, scaleOut);
                        ::eyelib::world::components::Transform newWorld;
                        newWorld.position = posOut;
                        newWorld.rotation = rotOut;
                        newWorld.scale = scaleOut;
                        ctx->world.SetWorldTransform(sel, newWorld);
                    }
                }
            }
            ImGui::End();
            return;
        }

        const bool canRender = ctx->sceneViewPipeline && ctx->assetRegistry && w > 0 && h > 0;
        const ::eyelib::render::RenderBackend backend = ctx->sceneViewPipeline
            ? ctx->sceneViewPipeline->GetBackend()
            : ::eyelib::render::RenderBackend::Default;

        eyelib::api::world::World::RenderSnapshot snapshot;
        if (canRender) {
            ctx->world.FlushTransforms();
            ctx->world.CaptureRenderSnapshot(snapshot);
        }

        std::vector<EntityId> opaqueEntityIds;
        std::vector<EntityId> transparentEntityIds;
        std::vector<uint32_t> entityIdsForOpaque;
        std::vector<uint32_t> entityIdsForTransparent;
        size_t drawableCount = 0;
        if (canRender) {
            eyelib::render::rhi::IDevice* rhiDev =
                static_cast<eyelib::render::rhi::IDevice*>(ctx->sceneViewPipeline->GetDevice().Get());
            ::openeye::editor::VulkanRenderSliceScope vulkanSlice(rhiDev);
            static EditorSceneBuilder s_builder;
            eyelib::api::RenderScene scene;
            eyelib::api::debug::Profiler::BeginSample("SceneView Build");
            s_builder.Build(scene, snapshot, ctx->editorCamera, w, h, ctx->assetRoots, ctx->assetRegistry, ctx->defaultMaterial, &ctx->world,
                &opaqueEntityIds, &transparentEntityIds);
            eyelib::api::debug::Profiler::EndSample();
            drawableCount = scene.opaqueCount + scene.transparentCount;
            for (EntityId e : opaqueEntityIds)
                entityIdsForOpaque.push_back(PackEntityId(e));
            for (EntityId e : transparentEntityIds)
                entityIdsForTransparent.push_back(PackEntityId(e));
            ctx->sceneViewPipeline->Resize(w, h);
            {
                Mat4 view, proj;
                BuildViewProjection(ctx->editorCamera, size.x, size.y, backend, view, proj);
                ColoredLines colored;
                CollectGridAndAxisLinesColored(ctx->editorCamera, colored);
                if (!snapshot.colliders.empty()) {
                    std::vector<std::pair<Vec3, Vec3>> colliderLines;
                    colliderLines.reserve(snapshot.colliders.size() * 64u);
                    CollectColliderDebugLines(snapshot, colliderLines);
                    const Vec4 green(0.f, 1.f, 0.f, 1.f);
                    for (const auto& line : colliderLines)
                        colored.push_back({ line, green });
                }
                ctx->sceneViewPipeline->SetPendingDebugLinesColored(view, proj, colored);
            }
            eyelib::api::debug::Profiler::BeginSample("SceneView Render");
            ctx->sceneViewPipeline->Render(scene, &entityIdsForOpaque, &entityIdsForTransparent);
            eyelib::api::debug::Profiler::EndSample();
            if (ctx->selectedEntity != ::eyelib::world::InvalidEntity && ctx->world.IsAlive(ctx->selectedEntity))
                ctx->sceneViewPipeline->RenderOutlineFromIdBuffer(PackEntityId(ctx->selectedEntity), 1.0f, 0.5f, 0.0f);
            void* rawTexId = ctx->sceneViewPipeline->GetOutputColorTextureNativeHandle();
            void* texId = rawTexId;
            if (ctx->resolveImGuiTextureId)
                texId = ctx->resolveImGuiTextureId(texId);
            if (texId && ctx->sceneViewPipeline->HasValidOutput()) {
                ImGui::GetWindowDrawList()->AddImage(texId, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    ImVec2(0, 0), ImVec2(1, 1));
            } else {
                ImDrawList* draw = ImGui::GetWindowDrawList();
                draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 42, 54, 255));
                const char* label = "Scene view (no output)";
                ImVec2 textSize = ImGui::CalcTextSize(label);
                draw->AddText(ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
                    IM_COL32(180, 180, 180, 255), label);
            }
        } else {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 42, 54, 255));
            const char* label = "Scene view";
            ImVec2 textSize = ImGui::CalcTextSize(label);
            draw->AddText(ImVec2(pos.x + (size.x - textSize.x) * 0.5f, pos.y + (size.y - textSize.y) * 0.5f),
                IM_COL32(180, 180, 180, 255), label);
        }

        // Light gizmos: icon + direction arrow only when a light is selected (select via Hierarchy)
        if (canRender && !snapshot.lights.empty()) {
            EntityId sel = ctx->selectedEntity;
            if (sel != ::eyelib::world::InvalidEntity && ctx->world.IsAlive(sel)) {
                Mat4 view, proj;
                BuildViewProjection(ctx->editorCamera, size.x, size.y, backend, view, proj);
                static LightGizmoIcons lightIcons;
                if (lightIcons.EnsureLoaded()) {
                    ImDrawList* draw = ImGui::GetWindowDrawList();
                    const float half = kLightIconSize * 0.5f;
                    using LT = ::eyelib::world::components::Light::Type;
                    for (const auto& lightEntry : snapshot.lights) {
                        if (lightEntry.entity != sel) continue;
                        auto optTransform = snapshot.GetTransform(lightEntry.entity);
                        if (!optTransform) continue;
                        const Vec3 worldPos(optTransform->position.x, optTransform->position.y, optTransform->position.z);
                        float sx = 0.f, sy = 0.f;
                        if (!WorldToScreen(worldPos, view, proj, pos.x, pos.y, size.x, size.y, sx, sy)) continue;
                        uint64_t texId = lightIcons.GetTexture(lightEntry.component.type);
                        if (texId) {
                            draw->AddImage(static_cast<ImTextureID>(texId),
                                ImVec2(sx - half, sy - half), ImVec2(sx + half, sy + half),
                                ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
                        }
                        if (lightEntry.component.type == LT::Directional || lightEntry.component.type == LT::Spot) {
                            Vec3 dir = DirectionFromTransform(*optTransform);
                            float arrowLen = (lightEntry.component.type == LT::Directional) ? kDirectionArrowLength : kSpotArrowLength;
                            Vec3 endWorld(worldPos.x + dir.x * arrowLen, worldPos.y + dir.y * arrowLen, worldPos.z + dir.z * arrowLen);
                            float ex = 0.f, ey = 0.f;
                            if (WorldToScreen(endWorld, view, proj, pos.x, pos.y, size.x, size.y, ex, ey))
                                draw->AddLine(ImVec2(sx, sy), ImVec2(ex, ey), kOrangeColor, kArrowThickness);
                        }
                        break;
                    }
                }
            }
        }

        // Gizmo for selected entity: operate in world space (Current_advice.md)
        EntityId sel = ctx->selectedEntity;
        if (sel != ::eyelib::world::InvalidEntity && ctx->world.IsAlive(sel)) {
            std::optional<::eyelib::world::components::Transform> worldOpt;
            if (canRender)
                worldOpt = snapshot.GetTransform(sel);
            else {
                ctx->world.FlushTransforms();
                ::eyelib::world::components::Transform worldTrTemp;
                if (ctx->world.GetWorldTransform(sel, worldTrTemp)) worldOpt = worldTrTemp;
            }
            if (worldOpt) {
                const auto& worldTr = *worldOpt;
                Mat4 view, proj;
                BuildViewProjection(ctx->editorCamera, size.x, size.y, backend, view, proj);
                Mat4 model = eyelib::math::Compose(worldTr.position, worldTr.rotation, worldTr.scale);
                float modelMatrix[16];
                std::memcpy(modelMatrix, &model[0][0], sizeof(modelMatrix));

                ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

                const ImGuizmo::OPERATION op = ImGuizmo::OPERATION::UNIVERSAL;  // translate + rotate + scale in one
                const ImGuizmo::MODE mode = ctx->sceneGizmoLocal ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD;
                if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0], op, mode, modelMatrix)) {
                    Vec3 posOut, scaleOut;
                    Quat rotOut;
                    eyelib::math::Decompose(*reinterpret_cast<Mat4*>(modelMatrix), posOut, rotOut, scaleOut);
                    ::eyelib::world::components::Transform newWorld;
                    newWorld.position = posOut;
                    newWorld.rotation = rotOut;
                    newWorld.scale = scaleOut;
                    ctx->world.SetWorldTransform(sel, newWorld);
                }
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + ImGui::GetStyle().ItemSpacing.y));
        if (canRender) {
            ImGui::Text("Drawables: %zu | WASD+QE move, RMB rotate | Gizmo: drag to translate/rotate/scale | Select in Hierarchy", drawableCount);
            if (drawableCount == 0)
                ImGui::TextColored(ImVec4(1.f, 0.8f, 0.4f, 1.f), "No mesh renderers in scene. Add Renderable/MeshRenderer and mesh path under assets/.");
        } else {
            ImGui::TextUnformatted("WASD+QE move, RMB drag rotate (no pipeline)");
        }

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            EditorCamera& cam = ctx->editorCamera;
            ImGui::DragFloat3("Position", cam.position, 0.02f);
            ImGui::DragFloat("Yaw", &cam.yaw, 0.01f);
            if (ImGui::DragFloat("Pitch", &cam.pitch, 0.01f, kPitchMin, kPitchMax)) {
                if (cam.pitch > kPitchMax) cam.pitch = kPitchMax;
                if (cam.pitch < kPitchMin) cam.pitch = kPitchMin;
            }
            ImGui::DragFloat("Move speed", &cam.moveSpeed, 0.1f, 0.5f, 50.0f);
            ImGui::DragFloat("Mouse sensitivity", &cam.mouseSensitivity, 0.0001f, 0.0005f, 0.02f, "%.4f");
        }
    }
    ImGui::End();
}

} // namespace openeye::editor::panels
