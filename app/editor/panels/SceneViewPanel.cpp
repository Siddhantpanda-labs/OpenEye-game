#include "SceneViewPanel.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "PanelCommon.h"

#include "api/render/RenderPipeline.h"
#include "api/render/RenderScene.h"
#include "api/world/All.h"

#include <imgui.h>
#include <cmath>

namespace openeye::editor::panels {

namespace {
const float kPitchMin = -1.55f;  // just below -pi/2 to avoid flip
const float kPitchMax =  1.55f;
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

        size_t drawableCount = 0;
        if (canRender) {
            eyelib::api::world::World::RenderSnapshot snapshot;
            ctx->world.CaptureRenderSnapshot(snapshot);
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

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + ImGui::GetStyle().ItemSpacing.y));
        if (canRender) {
            ImGui::Text("Drawables: %zu | WASD+QE move, RMB drag rotate", drawableCount);
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
