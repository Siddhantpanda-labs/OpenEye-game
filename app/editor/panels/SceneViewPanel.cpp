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
#include <limits>

namespace openeye::editor::panels {

namespace {
const float kPitchMin = -1.55f;  // just below -pi/2 to avoid flip
const float kPitchMax =  1.55f;
const float kPickAabbHalf = 0.5f;  // AABB half-size for picking (position +/- this)

using EntityId = ::eyelib::world::EntityId;
using Vec3 = eyelib::math::Vec3;
using Mat4 = eyelib::math::Mat4;
using Quat = eyelib::math::Quat;

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

// Ray-AABB intersection; returns t (>= 0) to hit, or negative if miss.
float RayAABB(const Vec3& rayOrigin, const Vec3& rayDir,
              const Vec3& aabbMin, const Vec3& aabbMax) {
    const float eps = 1e-7f;
    float tmin = -std::numeric_limits<float>::infinity();
    float tmax = std::numeric_limits<float>::infinity();
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(rayDir[i]) < eps) {
            if (rayOrigin[i] < aabbMin[i] || rayOrigin[i] > aabbMax[i]) return -1.f;
            continue;
        }
        float invD = 1.f / rayDir[i];
        float t0 = (aabbMin[i] - rayOrigin[i]) * invD;
        float t1 = (aabbMax[i] - rayOrigin[i]) * invD;
        if (invD < 0.f) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmin > tmax) return -1.f;
    }
    if (tmax < 0.f) return -1.f;
    return tmin >= 0.f ? tmin : tmax;
}

void PickEntityFromViewport(EditorContext* ctx,
                            const eyelib::api::world::World::RenderSnapshot& snapshot,
                            ImVec2 viewPos, ImVec2 viewSize, const EditorCamera& cam) {
    ImVec2 mouse = ImGui::GetIO().MousePos;
    if (mouse.x < viewPos.x || mouse.x > viewPos.x + viewSize.x ||
        mouse.y < viewPos.y || mouse.y > viewPos.y + viewSize.y)
        return;
    Mat4 view, proj;
    BuildViewProjection(cam, viewSize.x, viewSize.y, view, proj);
    Mat4 invViewProj = glm::inverse(proj * view);
    float ndcX = (mouse.x - viewPos.x) / viewSize.x * 2.f - 1.f;
    float ndcY = 1.f - (mouse.y - viewPos.y) / viewSize.y * 2.f;
    glm::vec4 farNDC(ndcX, ndcY, 1.f, 1.f);
    glm::vec4 farW = invViewProj * farNDC;
    farW /= farW.w;
    Vec3 rayOrigin(cam.position[0], cam.position[1], cam.position[2]);
    Vec3 rayEnd(farW.x, farW.y, farW.z);
    Vec3 rayDir = glm::normalize(rayEnd - rayOrigin);

    EntityId best = ::eyelib::world::InvalidEntity;
    float bestT = std::numeric_limits<float>::max();

    auto testEntity = [&](EntityId eid) {
        auto opt = snapshot.GetTransform(eid);
        if (!opt) return;
        Vec3 p(opt->position.x, opt->position.y, opt->position.z);
        Vec3 mn = p - Vec3(kPickAabbHalf);
        Vec3 mx = p + Vec3(kPickAabbHalf);
        float t = RayAABB(rayOrigin, rayDir, mn, mx);
        if (t >= 0.f && t < bestT) {
            bestT = t;
            best = eid;
        }
    };
    for (const auto& e : snapshot.renderables) { if (e.component.visible) testEntity(e.entity); }
    for (const auto& e : snapshot.meshRenderers) { if (e.component.visible) testEntity(e.entity); }

    ctx->selectedEntity = best;
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

        size_t drawableCount = 0;
        if (canRender) {
            static EditorSceneBuilder s_builder;
            eyelib::api::RenderScene scene;
            s_builder.Build(scene, snapshot, ctx->editorCamera, w, h, ctx->assetRoots, ctx->assetRegistry);
            drawableCount = scene.opaqueCount;
            ctx->sceneViewPipeline->Resize(w, h);
            ctx->sceneViewPipeline->Render(scene);
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

        // Picking: left-click in viewport (not when using gizmo) selects entity
        if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsUsing()) {
            if (canRender)
                PickEntityFromViewport(ctx, snapshot, pos, size, ctx->editorCamera);
            else
                ctx->selectedEntity = ::eyelib::world::InvalidEntity;
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
