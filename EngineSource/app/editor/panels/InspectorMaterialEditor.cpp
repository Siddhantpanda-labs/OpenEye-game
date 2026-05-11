#include "InspectorMaterialEditor.h"
#include "MaterialPreview.h"
#include "EditorContext.h"

#include "api/asset/AssetId.h"
#include "api/asset/AssetRegistry.h"
#include "api/asset/IGPUResourceResolver.h"
#include "api/asset/loaders/MaterialLoader.h"
#include "api/platform/All.h"
#include "api/render/Material.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/materials/MaterialDescriptor.h"
#include "eyelib/render/materials/MaterialShader.h"
#include "eyelib/math/Types.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace openeye::editor {

namespace {

namespace fs = eyelib::api::platform::filesystem;

struct TextureSlotDef {
    const char* label;
    const char* uniform;
};

static constexpr TextureSlotDef kTextureSlots[] = {
    { "Albedo", "u_AlbedoTexture" },
    { "Normal", "u_NormalTexture" },
    { "Metallic", "u_MetallicTexture" },
    { "Roughness", "u_RoughnessTexture" },
    { "Ambient occlusion", "u_AOTexture" },
    { "Emissive", "u_EmissiveTexture" },
    { "Metallic + roughness (ORM)", "u_MetallicRoughnessTexture" },
};

static std::string AbsoluteMaterialPath(const EditorContext* ctx) {
    if (!ctx || ctx->projectAssetsRoot.empty() || ctx->selectedMaterialAssetId.empty()) return {};
    std::string root = fs::NormalizePath(ctx->projectAssetsRoot);
    std::string rel = ctx->selectedMaterialAssetId;
    std::replace(rel.begin(), rel.end(), '\\', '/');
    while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
    if (!root.empty() && root.back() != '/' && root.back() != '\\') root.push_back('/');
    return fs::NormalizePath(root + rel);
}

// M3.2 — author-time material editing posture during the dual-stack window:
//
// `MaterialDescriptor` is an editor-side authoring data model (loaded from `.mat`,
// mutated in the inspector, serialized back to disk). It is intentionally renderer-
// agnostic — there is no parallel `eyelib::api::rendering::MaterialDescriptor` because
// the new module's `MaterialInstance` is a *runtime* token, not an authoring record.
//
// Edits flow legacy → registry → legacy `Material` GPU object (this function). The new-
// path `MaterialPreview` cold branch (M3.1) currently renders with placeholder material
// tokens, so live preview reflects the legacy GPU state; that becomes "live new-side
// preview" automatically when M3.7 lands `EditorGPUResourceCacheNew::ApplyMaterialEdit`,
// which will call this function *and* mirror the descriptor onto the new-side material
// token (the only place an authoring → renderer translation can live without dragging
// the entire `.mat` schema into `api/rendering`).
//
// Until M3.7, this function intentionally remains the single mutation point — splitting
// it now would force the inspector to know about both pipelines and would require the
// new façade to expose an authoring surface it does not have.
static void ApplyDescriptorToGpu(EditorContext* ctx, const eyelib::api::AssetId& id,
                                 const eyelib::render::MaterialDescriptor& desc) {
    if (!ctx || !ctx->assetRegistry || !id.IsValid()) return;
    ctx->assetRegistry->SetMaterialDescriptor(id, desc);
    std::shared_ptr<eyelib::api::Material> apiMat = ctx->assetRegistry->GetMaterial(id);
    if (!apiMat) return;
    std::shared_ptr<eyelib::render::Material> mat = apiMat->GetInternal();
    if (!mat) return;
    eyelib::api::IGPUResourceResolver* resolver = ctx->assetRegistry->GetGPUResolver();
    auto resolveShader = [ctx, resolver](const std::string& name) -> std::shared_ptr<eyelib::render::Shader> {
        if (!ctx->assetRegistry || !resolver) return nullptr;
        eyelib::api::AssetId sid = ctx->assetRegistry->GetId("name:" + name);
        if (!sid.IsValid()) sid = ctx->assetRegistry->LoadAssetDescriptorOnly("name:" + name, eyelib::api::AssetType::Shader);
        return resolver->GetShader(sid);
    };
    auto resolveTexture = [ctx, resolver](const std::string& textureAssetGuid) -> std::shared_ptr<eyelib::render::Texture2D> {
        if (!ctx->assetRegistry || !resolver || textureAssetGuid.empty()) return nullptr;
        eyelib::api::AssetId tid = eyelib::api::AssetId::FromString(textureAssetGuid);
        if (!tid.IsValid()) return nullptr;
        return resolver->GetTexture(tid);
    };
    eyelib::render::ApplyMaterialFromDescriptor(*mat, desc, resolveShader, resolveTexture);
}

static void SectionTitle(const char* title) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.78f, 1.0f, 1.0f));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

}  // namespace

template <typename M>
static std::vector<std::string> SortedMapKeys(const M& m) {
    std::vector<std::string> ks;
    ks.reserve(m.size());
    for (const auto& kv : m) ks.push_back(kv.first);
    std::sort(ks.begin(), ks.end());
    return ks;
}

void DrawMaterialAssetEditor(EditorContext* ctx) {
    if (!ctx || ctx->selectedMaterialAssetId.empty() || !ctx->assetRegistry) return;

    eyelib::api::AssetId matId = ctx->assetRegistry->GetId(ctx->selectedMaterialAssetId);
    if (!matId.IsValid()) {
        const std::string absPath = AbsoluteMaterialPath(ctx);
        if (absPath.empty() || !fs::FileExists(absPath)) {
            ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "Material is not registered. Scan project Assets or check path.");
            ImGui::TextWrapped("%s", ctx->selectedMaterialAssetId.c_str());
            return;
        }
    }

    ctx->assetRegistry->LoadAsset(ctx->selectedMaterialAssetId, eyelib::api::AssetType::Material);
    matId = ctx->assetRegistry->GetId(ctx->selectedMaterialAssetId);
    if (!matId.IsValid()) {
        ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "Failed to register or load material.");
        ImGui::TextWrapped("%s", ctx->selectedMaterialAssetId.c_str());
        return;
    }
    const eyelib::render::MaterialDescriptor* loaded = ctx->assetRegistry->GetMaterialDescriptor(matId);
    if (!loaded) {
        ImGui::TextColored(ImVec4(1, 0.45f, 0.35f, 1), "Failed to load material descriptor.");
        return;
    }

    static std::string s_lastKey;
    static eyelib::render::MaterialDescriptor s_edit{};
    static char s_pathBuf[7][512]{};
    static char s_fragSnippet[4096]{};
    static char s_shadeSnippet[4096]{};
    static char s_newKeyword[128]{};
    static bool s_newKeywordOn = true;
    static char s_shaderPathBuf[512]{};
    static char s_newFloatKey[160]{};
    static float s_newFloatVal = 0.f;
    static char s_newVec3Key[160]{};
    static float s_newVec3[3]{};
    static char s_newVec4Key[160]{};
    static float s_newVec4[4]{ 1.f, 1.f, 1.f, 1.f };

    const std::string key = ctx->selectedMaterialAssetId;
    if (key != s_lastKey) {
        s_lastKey = key;
        s_edit = *loaded;
        constexpr size_t kNumSlots = sizeof(kTextureSlots) / sizeof(kTextureSlots[0]);
        for (size_t i = 0; i < kNumSlots; ++i) {
            auto it = s_edit.texturePaths.find(kTextureSlots[i].uniform);
            const std::string& p = it != s_edit.texturePaths.end() ? it->second : std::string();
            std::snprintf(s_pathBuf[i], 512, "%.511s", p.c_str());
        }
        std::snprintf(s_fragSnippet, sizeof(s_fragSnippet), "%.4095s", s_edit.fragmentSnippet.c_str());
        std::snprintf(s_shadeSnippet, sizeof(s_shadeSnippet), "%.4095s", s_edit.shadingSnippet.c_str());
        std::snprintf(s_shaderPathBuf, sizeof(s_shaderPathBuf), "%.511s", s_edit.shaderAssetPath.c_str());
    }

    auto pushApply = [&] { ApplyDescriptorToGpu(ctx, matId, s_edit); };

    constexpr float kPreviewDockHeight = 300.f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.9f, 0.96f, 1.f));
    ImGui::TextWrapped("%s", ctx->selectedMaterialAssetId.c_str());
    ImGui::PopStyleColor();
    ImGui::TextDisabled("Material asset · edits apply to GPU; Save writes MAT1 to disk.");
    ImGui::Separator();

    ImGui::BeginChild("MatEditorScroll", ImVec2(0, -kPreviewDockHeight), false,
        ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::BeginGroup();
    if (ImGui::Button("Save to disk", ImVec2(120, 0))) {
        const std::string abs = AbsoluteMaterialPath(ctx);
        if (abs.empty()) {
            ImGui::OpenPopup("MatSaveErr");
        } else if (eyelib::api::MaterialLoader::SaveMaterial(abs, s_edit)) {
            ctx->assetRegistry->ReloadAsset(matId);
            s_lastKey.clear();
        } else {
            ImGui::OpenPopup("MatSaveErr");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload from disk", ImVec2(140, 0))) {
        ctx->assetRegistry->UnloadAsset(matId);
        ctx->assetRegistry->LoadAsset(ctx->selectedMaterialAssetId, eyelib::api::AssetType::Material);
        s_lastKey.clear();
    }
    ImGui::EndGroup();

    if (ImGui::BeginPopupModal("MatSaveErr", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Could not save material (path or permissions).");
        ImGui::TextUnformatted(AbsoluteMaterialPath(ctx).c_str());
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    SectionTitle("Shader asset path");
    {
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("Override##shaderAssetPath", s_shaderPathBuf, sizeof(s_shaderPathBuf),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
            s_edit.shaderAssetPath = s_shaderPathBuf;
            pushApply();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            s_edit.shaderAssetPath = s_shaderPathBuf;
            pushApply();
        }
        ImGui::TextDisabled(
            "Optional asset path or id for the forward shader. Empty uses material shader id + pipeline defaults. "
            "Saved in the MATX trailer when non-empty.");
    }

    SectionTitle("Shading & render state");
    {
        const char* smItems[] = { "Lit (PBR)", "Unlit", "Custom" };
        int sm = (s_edit.shadingModel == eyelib::render::ShadingModel::Lit)     ? 0
               : (s_edit.shadingModel == eyelib::render::ShadingModel::Unlit)   ? 1
                                                                                 : 2;
        ImGui::SetNextItemWidth(-120);
        if (ImGui::Combo("Shading model", &sm, smItems, IM_ARRAYSIZE(smItems))) {
            s_edit.shadingModel = (sm == 0) ? eyelib::render::ShadingModel::Lit
                               : (sm == 1) ? eyelib::render::ShadingModel::Unlit
                                           : eyelib::render::ShadingModel::Custom;
            if (s_edit.shadingModel == eyelib::render::ShadingModel::Unlit)
                s_edit.materialShaderId = eyelib::render::MaterialShaderIds::Unlit;
            else if (s_edit.shadingModel == eyelib::render::ShadingModel::Lit)
                s_edit.materialShaderId = eyelib::render::kDefaultMaterialTypeId;
            pushApply();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Lit uses metallic-roughness lighting. Unlit skips lights. Custom uses snippets below.");

        const char* blendItems[] = { "Opaque", "Translucent", "Additive", "Modulate" };
        int bl = static_cast<int>(s_edit.blending);
        if (bl < 0 || bl > 3) bl = 0;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("Blending", &bl, blendItems, IM_ARRAYSIZE(blendItems))) {
            s_edit.blending = static_cast<eyelib::render::Blending>(bl);
            pushApply();
        }

        bool two = s_edit.twoSided;
        if (ImGui::Checkbox("Two-sided (no backface cull)", &two)) {
            s_edit.twoSided = two;
            pushApply();
        }

        uint64_t sid = static_cast<uint64_t>(s_edit.materialShaderId);
        if (ImGui::DragScalar("Material shader id", ImGuiDataType_U64, &sid, 1.f, nullptr, nullptr)) {
            s_edit.materialShaderId = static_cast<eyelib::render::MaterialTypeId>(sid);
            pushApply();
        }
        ImGui::TextDisabled("Built-in: 0 = PBR, 1 = Unlit");
    }

    SectionTitle("Surface (constants)");
    {
        float alb[3] = { s_edit.albedo.x, s_edit.albedo.y, s_edit.albedo.z };
        if (ImGui::ColorEdit3("Albedo / base color", alb)) {
            s_edit.albedo.x = alb[0];
            s_edit.albedo.y = alb[1];
            s_edit.albedo.z = alb[2];
            pushApply();
        }
        if (ImGui::SliderFloat("Metallic", &s_edit.metallic, 0.f, 1.f, "%.2f")) pushApply();
        if (ImGui::SliderFloat("Roughness", &s_edit.roughness, 0.f, 1.f, "%.2f")) pushApply();
        if (ImGui::SliderFloat("Ambient occlusion", &s_edit.ao, 0.f, 1.f, "%.2f")) pushApply();
        if (ImGui::SliderFloat("Alpha", &s_edit.alpha, 0.f, 1.f, "%.2f")) pushApply();
        float em[3] = { s_edit.emission.x, s_edit.emission.y, s_edit.emission.z };
        if (ImGui::ColorEdit3("Emission (HDR)", em, ImGuiColorEditFlags_Float)) {
            s_edit.emission.x = em[0];
            s_edit.emission.y = em[1];
            s_edit.emission.z = em[2];
            pushApply();
        }
    }

    SectionTitle("Textures (texture asset GUID from .meta)");
    for (size_t i = 0; i < sizeof(kTextureSlots) / sizeof(kTextureSlots[0]); ++i) {
        const TextureSlotDef& slot = kTextureSlots[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText(slot.label, s_pathBuf[i], sizeof(s_pathBuf[i]),
                ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string v(s_pathBuf[i]);
            if (v.empty())
                s_edit.texturePaths.erase(slot.uniform);
            else
                s_edit.texturePaths[slot.uniform] = v;
            pushApply();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string v(s_pathBuf[i]);
            if (v.empty())
                s_edit.texturePaths.erase(slot.uniform);
            else
                s_edit.texturePaths[slot.uniform] = v;
            pushApply();
        }
        ImGui::PopID();
    }

    SectionTitle("Effects");
    {
        bool refr = s_edit.refraction;
        if (ImGui::Checkbox("Refraction (shader keyword)", &refr)) {
            s_edit.refraction = refr;
            pushApply();
        }
        float refl = s_edit.reflectiveStrength;
        if (ImGui::SliderFloat("Screen-space reflections", &refl, 0.f, 1.f, "%.2f")) {
            s_edit.reflectiveStrength = refl;
            pushApply();
        }
        ImGui::TextDisabled("SSR blends with scene; use roughness for blur.");
    }

    SectionTitle("Shader snippets (Custom shading)");
    {
        ImGui::TextUnformatted("Fragment snippet");
        if (ImGui::InputTextMultiline("##frag", s_fragSnippet, sizeof(s_fragSnippet), ImVec2(-1, 72))) {
            s_edit.fragmentSnippet = s_fragSnippet;
            pushApply();
        }
        ImGui::TextUnformatted("Shading snippet");
        if (ImGui::InputTextMultiline("##shade", s_shadeSnippet, sizeof(s_shadeSnippet), ImVec2(-1, 72))) {
            s_edit.shadingSnippet = s_shadeSnippet;
            pushApply();
        }
    }

    SectionTitle("Keywords");
    {
        std::vector<std::string> keys;
        keys.reserve(s_edit.keywords.size());
        for (const auto& kv : s_edit.keywords) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (const std::string& k : keys) {
            ImGui::PushID(k.c_str());
            bool on = s_edit.keywords[k];
            if (ImGui::Checkbox(k.c_str(), &on)) {
                s_edit.keywords[k] = on;
                pushApply();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                s_edit.keywords.erase(k);
                pushApply();
            }
            ImGui::PopID();
        }
        ImGui::SetNextItemWidth(-80);
        ImGui::InputText("##newkw", s_newKeyword, sizeof(s_newKeyword));
        ImGui::SameLine();
        ImGui::Checkbox("On##kw", &s_newKeywordOn);
        ImGui::SameLine();
        if (ImGui::Button("Add keyword") && s_newKeyword[0] != '\0') {
            s_edit.keywords[std::string(s_newKeyword)] = s_newKeywordOn;
            s_newKeyword[0] = '\0';
            pushApply();
        }
    }

    SectionTitle("Custom uniforms (saved in MATX)");
    {
        ImGui::TextUnformatted("Float");
        for (const std::string& k : SortedMapKeys(s_edit.floats)) {
            ImGui::PushID(("f" + k).c_str());
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(k.c_str());
            ImGui::SameLine(180);
            float v = s_edit.floats[k];
            ImGui::SetNextItemWidth(-60);
            if (ImGui::DragFloat("##fv", &v, 0.02f)) {
                s_edit.floats[k] = v;
                pushApply();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                s_edit.floats.erase(k);
                pushApply();
            }
            ImGui::PopID();
        }
        ImGui::SetNextItemWidth(140);
        ImGui::InputText("##nfk", s_newFloatKey, sizeof(s_newFloatKey));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("##nfv", &s_newFloatVal, 0.02f);
        ImGui::SameLine();
        if (ImGui::Button("Add float") && s_newFloatKey[0] != '\0') {
            s_edit.floats[std::string(s_newFloatKey)] = s_newFloatVal;
            s_newFloatKey[0] = '\0';
            pushApply();
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Vec3");
        for (const std::string& k : SortedMapKeys(s_edit.vec3s)) {
            ImGui::PushID(("v3" + k).c_str());
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(k.c_str());
            ImGui::SameLine(180);
            auto& vv = s_edit.vec3s[k];
            float a[3] = { vv.x, vv.y, vv.z };
            ImGui::SetNextItemWidth(-60);
            if (ImGui::DragFloat3("##v3", a, 0.02f)) {
                vv.x = a[0];
                vv.y = a[1];
                vv.z = a[2];
                pushApply();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                s_edit.vec3s.erase(k);
                pushApply();
            }
            ImGui::PopID();
        }
        ImGui::SetNextItemWidth(140);
        ImGui::InputText("##n3k", s_newVec3Key, sizeof(s_newVec3Key));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-80);
        ImGui::DragFloat3("##n3v", s_newVec3, 0.02f);
        ImGui::SameLine();
        if (ImGui::Button("Add vec3") && s_newVec3Key[0] != '\0') {
            s_edit.vec3s[std::string(s_newVec3Key)] =
                eyelib::math::Vec3(s_newVec3[0], s_newVec3[1], s_newVec3[2]);
            s_newVec3Key[0] = '\0';
            pushApply();
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Vec4");
        for (const std::string& k : SortedMapKeys(s_edit.vec4s)) {
            ImGui::PushID(("v4" + k).c_str());
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(k.c_str());
            ImGui::SameLine(180);
            auto& vv = s_edit.vec4s[k];
            float a[4] = { vv.x, vv.y, vv.z, vv.w };
            ImGui::SetNextItemWidth(-60);
            if (ImGui::DragFloat4("##v4", a, 0.02f)) {
                vv.x = a[0];
                vv.y = a[1];
                vv.z = a[2];
                vv.w = a[3];
                pushApply();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                s_edit.vec4s.erase(k);
                pushApply();
            }
            ImGui::PopID();
        }
        ImGui::SetNextItemWidth(140);
        ImGui::InputText("##n4k", s_newVec4Key, sizeof(s_newVec4Key));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-80);
        ImGui::DragFloat4("##n4v", s_newVec4, 0.02f);
        ImGui::SameLine();
        if (ImGui::Button("Add vec4") && s_newVec4Key[0] != '\0') {
            s_edit.vec4s[std::string(s_newVec4Key)] =
                eyelib::math::Vec4(s_newVec4[0], s_newVec4[1], s_newVec4[2], s_newVec4[3]);
            s_newVec4Key[0] = '\0';
            pushApply();
        }

        ImGui::Spacing();
        ImGui::TextDisabled(
            "mat4[] arrays are supported on the material at runtime only; they are not written to the .mat file yet.");
    }

    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    SectionTitle("Viewport preview");
    ImGui::TextDisabled("Lit sphere with turntable orbit. Uses the live material in the registry.");
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.056f, 0.07f, 1.f));
    ImGui::BeginChild("MatPreviewRegion", ImVec2(0, 0),
        ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
    DrawMaterialEditorPreview(ctx, matId);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
}

}  // namespace openeye::editor
