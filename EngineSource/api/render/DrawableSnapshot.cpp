#include "api/render/RenderScene.h"
#include "api/render/Texture.h"
#include "eyelib/render/materials/Material.h"

namespace eyelib {
namespace api {

static std::shared_ptr<Texture> GetTextureFromMaterial(const std::shared_ptr<Material>& material,
                                                        const std::shared_ptr<MaterialInstance>& materialInstance,
                                                        const std::string& name) {
    if (materialInstance) {
        auto t = materialInstance->GetTexture(name);
        return t;
    }
    if (material) {
        auto internal = material->GetInternal();
        if (internal) {
            auto tex2d = internal->GetTexture(name);
            return tex2d ? Texture::WrapInternal(tex2d) : nullptr;
        }
    }
    return nullptr;
}

void FillDrawableRenderParams(DrawableRenderParams& out,
                              const std::shared_ptr<Material>& material,
                              const std::shared_ptr<MaterialInstance>& materialInstance) {
    if (!material) return;

    out.albedo = materialInstance ? materialInstance->GetAlbedo() : material->GetAlbedo();
    out.metallic = materialInstance ? materialInstance->GetMetallic() : material->GetMetallic();
    out.roughness = materialInstance ? materialInstance->GetRoughness() : material->GetRoughness();
    out.ao = materialInstance ? materialInstance->GetAO() : material->GetAO();
    out.emission = materialInstance ? materialInstance->GetEmission() : material->GetEmission();
    out.alpha = materialInstance ? materialInstance->GetAlpha() : material->GetAlpha();

    const math::Vec4 kDefaultUV(0.0f, 0.0f, 1.0f, 1.0f);
    out.uvTransform = materialInstance ? materialInstance->GetVec4("u_UVTransform", kDefaultUV) : material->GetVec4("u_UVTransform", kDefaultUV);
    out.uvRotation = materialInstance ? materialInstance->GetFloat("u_UVRotation", 0.0f) : material->GetFloat("u_UVRotation", 0.0f);
    out.texCoordIndex = materialInstance ? materialInstance->GetFloat("u_TexCoordIndex", 0.0f) : material->GetFloat("u_TexCoordIndex", 0.0f);
    out.flipV = materialInstance ? materialInstance->GetFloat("u_FlipV", 0.0f) : material->GetFloat("u_FlipV", 0.0f);

    out.albedoTexture = GetTextureFromMaterial(material, materialInstance, "u_AlbedoTexture");
    out.normalTexture = GetTextureFromMaterial(material, materialInstance, "u_NormalTexture");
    out.metallicRoughnessTexture = GetTextureFromMaterial(material, materialInstance, "u_MetallicRoughnessTexture");
    out.aoTexture = GetTextureFromMaterial(material, materialInstance, "u_AOTexture");
    out.emissiveTexture = GetTextureFromMaterial(material, materialInstance, "u_EmissiveTexture");
    out.metallicTexture = GetTextureFromMaterial(material, materialInstance, "u_MetallicTexture");
    out.roughnessTexture = GetTextureFromMaterial(material, materialInstance, "u_RoughnessTexture");
}

} // namespace api
} // namespace eyelib
