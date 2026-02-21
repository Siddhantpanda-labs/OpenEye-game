#include "SceneViewPanel.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "PanelCommon.h"

#include "api/render/RenderPipeline.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"
#include "api/math/TransformUtils.h"

#include "ImGuizmo.h"

#include <imgui.h>
#include <json.hpp>
#include <cmath>
#include <cstring>

namespace openeye::editor::panels {

namespace {
const float kPitchMin = -1.55f;  // just below -pi/2 to avoid flip
const float kPitchMax =  1.55f;

using EntityId = ::eyelib::world::EntityId;
using Vec3 = eyelib::math::Vec3;
using Mat4 = eyelib::math::Mat4;
using Quat = eyelib::math::Quat;

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

void BuildViewProjection(const EditorCamera& cam, float viewportW, float viewportH,
                         Mat4& outView, Mat4& outProj) {
    const float aspect = (viewportH > 0.f) ? (viewportW / viewportH) : 1.f;
    outProj = eyelib::math::Perspective(
        static_cast<float>(cam.fovDegrees * 3.14159265 / 180.0), aspect, 0.1f, 1000.f);
    Vec3 position(cam.position[0], cam.position[1], cam.position[2]);
    float cy = std::cos(cam.pitch), sy = std::sin(cam.pitch);
    float ct = std::cos(cam.yaw), st = std::sin(cam.yaw);
    Vec3 forward(cy * st, sy, -cy * ct);
    Vec3 target = position + forward;
    outView = eyelib::math::LookAt(position, target, Vec3(0.f, 1.f, 0.f));
}
}

void ShowSceneViewPanel(EditorContext* ctx) {
    if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_None)) {
        if (!ctx) {
            ImGui::TextUnformatted("(No context)");
            ImGui::End();
            return;
        }

        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x < 64.0f) size.x = 64.0f;
        if (size.y < 64.0f) size.y = 64.0f;

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##sceneviewport", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

        const bool viewportHovered = ImGui::IsItemHovered();
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
        const bool canRender = ctx->sceneViewPipeline && ctx->assetRegistry && w > 0 && h > 0;

        eyelib::api::world::World::RenderSnapshot snapshot;
        if (canRender)
            ctx->world.CaptureRenderSnapshot(snapshot);

        std::vector<EntityId> drawableToEntity;
        std::vector<uint32_t> entityIdsForOpaque;
        size_t drawableCount = 0;
        if (canRender) {
            static EditorSceneBuilder s_builder;
            eyelib::api::RenderScene scene;
            s_builder.Build(scene, snapshot, ctx->editorCamera, w, h, ctx->assetRoots, ctx->assetRegistry, &drawableToEntity);
            drawableCount = scene.opaqueCount;
            for (EntityId e : drawableToEntity)
                entityIdsForOpaque.push_back(PackEntityId(e));
            ctx->sceneViewPipeline->Resize(w, h);
            ctx->sceneViewPipeline->Render(scene, entityIdsForOpaque.empty() ? nullptr : &entityIdsForOpaque);
            if (ctx->selectedEntity != ::eyelib::world::InvalidEntity && ctx->world.IsAlive(ctx->selectedEntity))
                ctx->sceneViewPipeline->RenderOutlineFromIdBuffer(PackEntityId(ctx->selectedEntity), 1.0f, 0.5f, 0.0f);
            void* texId = ctx->sceneViewPipeline->GetOutputColorTextureNativeHandle();
            if (texId && ctx->sceneViewPipeline->HasValidOutput()) {
                ImGui::GetWindowDrawList()->AddImage(texId, pos, ImVec2(pos.x + size.x, pos.y + size.y),
                    ImVec2(0, 1), ImVec2(1, 0));
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

        // Picking: left-click in viewport (not when using gizmo) — read entity ID from GPU buffer
        if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing()) {
            if (canRender && !entityIdsForOpaque.empty()) {
                ImVec2 mouse = ImGui::GetIO().MousePos;
                float px = mouse.x - pos.x;
                float py = mouse.y - pos.y;
                if (px >= 0.f && px < size.x && py >= 0.f && py < size.y) {
                    uint32_t pxu = static_cast<uint32_t>(px);
                    uint32_t pyu = static_cast<uint32_t>(py);
                    if (pxu >= w) pxu = w > 0u ? w - 1u : 0u;
                    if (pyu >= h) pyu = h > 0u ? h - 1u : 0u;
                    uint32_t rtY = (h > 0u) ? (h - 1u - pyu) : 0u;
                    ctx->sceneViewPipeline->Finish();
                    uint32_t id = 0;
                    if (ctx->sceneViewPipeline->ReadPixelEntityId(pxu, rtY, &id))
                        ctx->selectedEntity = UnpackEntityId(id);
                    else
                        ctx->selectedEntity = ::eyelib::world::InvalidEntity;
                }
            } else {
                ctx->selectedEntity = ::eyelib::world::InvalidEntity;
            }
        }

        // Gizmo for selected entity
        EntityId sel = ctx->selectedEntity;
        if (sel != ::eyelib::world::InvalidEntity && ctx->world.IsAlive(sel)) {
            ::eyelib::world::components::Transform tr;
            if (ctx->world.GetTransform(sel, tr)) {
                Mat4 view, proj;
                BuildViewProjection(ctx->editorCamera, size.x, size.y, view, proj);
                Mat4 model = eyelib::math::Compose(tr.position, tr.rotation, tr.scale);
                float modelMatrix[16];
                std::memcpy(modelMatrix, &model[0][0], sizeof(modelMatrix));

                ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
                ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

                static ImGuizmo::OPERATION op = ImGuizmo::OPERATION::TRANSLATE;
                if (ImGui::IsKeyPressed(ImGuiKey_T)) op = ImGuizmo::OPERATION::TRANSLATE;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) op = ImGuizmo::OPERATION::ROTATE;
                if (ImGui::IsKeyPressed(ImGuiKey_G)) op = ImGuizmo::OPERATION::SCALE;

                if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0], op, ImGuizmo::MODE::WORLD, modelMatrix)) {
                    Vec3 posOut, scaleOut;
                    Quat rotOut;
                    eyelib::math::Decompose(*reinterpret_cast<Mat4*>(modelMatrix), posOut, rotOut, scaleOut);
                    nlohmann::json j;
                    j["position"] = { posOut.x, posOut.y, posOut.z };
                    j["rotation"] = { rotOut.w, rotOut.x, rotOut.y, rotOut.z };
                    j["scale"] = { scaleOut.x, scaleOut.y, scaleOut.z };
                    ctx->world.SetComponentJson(sel, "Transform", j.dump());
                }
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + ImGui::GetStyle().ItemSpacing.y));
        if (canRender) {
            ImGui::Text("Drawables: %zu | WASD+QE move, RMB rotate | T/R/G gizmo | Click to select", drawableCount);
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
