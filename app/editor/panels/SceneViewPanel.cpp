#include "SceneViewPanel.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "LightGizmoIcons.h"
#include "PanelCommon.h"

#include "api/render/RenderPipeline.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"
#include "api/math/TransformUtils.h"

#include "eyelib/world/components/Light.h"

#include "ImGuizmo.h"

#include <imgui.h>
#include <cmath>
#include <cstring>

namespace openeye::editor::panels {

namespace {
const float kPitchMin = -1.55f;  // just below -pi/2 to avoid flip
const float kPitchMax =  1.55f;

using EntityId = ::eyelib::world::EntityId;
using Vec3 = eyelib::math::Vec3;
using Vec4 = eyelib::math::Vec4;
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
} // namespace

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

        // Light gizmos: icon + direction arrow only when a light is selected (select via Hierarchy)
        if (canRender && !snapshot.lights.empty()) {
            EntityId sel = ctx->selectedEntity;
            if (sel != ::eyelib::world::InvalidEntity && ctx->world.IsAlive(sel)) {
                Mat4 view, proj;
                BuildViewProjection(ctx->editorCamera, size.x, size.y, view, proj);
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

                const ImGuizmo::OPERATION op = ImGuizmo::OPERATION::UNIVERSAL;  // translate + rotate + scale in one
                if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0], op, ImGuizmo::MODE::WORLD, modelMatrix)) {
                    Vec3 posOut, scaleOut;
                    Quat rotOut;
                    eyelib::math::Decompose(*reinterpret_cast<Mat4*>(modelMatrix), posOut, rotOut, scaleOut);
                    ::eyelib::world::components::Transform trOut;
                    trOut.position = posOut;
                    trOut.rotation = rotOut;
                    trOut.scale = scaleOut;
                    ctx->world.SetTransform(sel, trOut);
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
