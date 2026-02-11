#include "MaterialLoader.h"

#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/loaders/ShaderLoader.h"
#include "eyelib/assets/loaders/TextureLoader.h"
#include "eyelib/core/Log.h"
#include "eyelib/math/Types.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Texture2D.h"

#include <json.hpp>

#include <algorithm>
#include <fstream>
#include <string>

namespace eyelib {
namespace assets {

namespace {

std::string DirOf(const std::string& path) {
    const std::string::size_type i = path.find_last_of("/\\");
    if (i == std::string::npos) return {};
    return path.substr(0, i + 1);
}

std::string ResolvePath(const std::string& baseDir, const std::string& value) {
    if (value.empty()) return value;
    if (value.size() >= 2 && value[0] == '.' && (value[1] == '/' || value[1] == '\\'))
        return baseDir + value.substr(2);
    if (baseDir.empty()) return value;
    return baseDir + value;
}

bool ParseVec3(const nlohmann::json& j, math::Vec3& out) {
    if (!j.is_array() || j.size() != 3) return false;
    out.x = j[0].get<float>();
    out.y = j[1].get<float>();
    out.z = j[2].get<float>();
    return true;
}

} // namespace

AssetId MaterialLoader::LoadMaterial(const std::string& path) {
    auto& manager = AssetManager::Get();
    const AssetId id = manager.Registry().RegisterAsset(path, "Material");

    std::ifstream f(path);
    if (!f.is_open()) {
        core::Log::WarnF("MaterialLoader: cannot open %s", path.c_str());
        return AssetId::Invalid();
    }
    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        core::Log::WarnF("MaterialLoader: JSON parse error %s: %s", path.c_str(), e.what());
        return AssetId::Invalid();
    }

    const std::string baseDir = DirOf(path);
    const int version = j.value("version", 1);
    (void)version;

    std::shared_ptr<render::Material> mat = std::make_shared<render::Material>();

    if (j.contains("shader") && j["shader"].is_string()) {
        std::string shaderName = j["shader"].get<std::string>();
        if (!shaderName.empty()) {
            // Logical name (e.g. pass/forward/lit): backend resolves to opengl/ or diligent/
            if (shaderName.size() >= 5 && shaderName.compare(shaderName.size() - 5, 5, ".vert") == 0)
                shaderName.resize(shaderName.size() - 5);
            else if (shaderName.size() >= 5 && shaderName.compare(shaderName.size() - 5, 5, ".frag") == 0)
                shaderName.resize(shaderName.size() - 5);
            const AssetId shaderId = ShaderLoader::LoadShaderByName(shaderName);
            auto shader = manager.Cache().Get<render::Shader>(shaderId);
            if (shader) mat->SetShader(shader);
        }
    }

    if (j.contains("textures") && j["textures"].is_object()) {
        auto& reg = manager.Registry();
        for (auto it = j["textures"].begin(); it != j["textures"].end(); ++it) {
            if (!it.value().is_string()) continue;
            const std::string slot = it.key();
            std::string val = it.value().get<std::string>();
            std::shared_ptr<render::Texture2D> tex;
            if (val.size() > 8 && val.compare(0, 8, "assetId:") == 0) {
                const AssetId tid = AssetId::FromString(val.substr(8));
                if (tid.IsValid()) {
                    reg.LoadAsset(tid);
                    tex = manager.Cache().Get<render::Texture2D>(tid);
                }
            } else {
                const std::string resolved = ResolvePath(baseDir, val);
                const AssetId tid = TextureLoader::LoadTexture(resolved, true);
                if (tid.IsValid()) tex = manager.Cache().Get<render::Texture2D>(tid);
            }
            if (tex) mat->SetTexture(slot, tex);
        }
    }

    if (j.contains("albedo")) { math::Vec3 v; if (ParseVec3(j["albedo"], v)) mat->SetAlbedo(v); }
    if (j.contains("metallic") && j["metallic"].is_number()) mat->SetMetallic(j["metallic"].get<float>());
    if (j.contains("roughness") && j["roughness"].is_number()) mat->SetRoughness(j["roughness"].get<float>());
    if (j.contains("emission")) { math::Vec3 v; if (ParseVec3(j["emission"], v)) mat->SetEmission(v); }
    if (j.contains("ao") && j["ao"].is_number()) mat->SetAO(j["ao"].get<float>());
    if (j.contains("alpha") && j["alpha"].is_number()) mat->SetAlpha(j["alpha"].get<float>());

    if (j.contains("floats") && j["floats"].is_object()) {
        for (auto it = j["floats"].begin(); it != j["floats"].end(); ++it)
            if (it.value().is_number()) mat->SetFloat(it.key(), it.value().get<float>());
    }
    if (j.contains("vec3s") && j["vec3s"].is_object()) {
        for (auto it = j["vec3s"].begin(); it != j["vec3s"].end(); ++it) {
            math::Vec3 v;
            if (ParseVec3(it.value(), v)) mat->SetVec3(it.key(), v);
        }
    }
    if (j.contains("vec4s") && j["vec4s"].is_object()) {
        for (auto it = j["vec4s"].begin(); it != j["vec4s"].end(); ++it) {
            if (!it.value().is_array() || it.value().size() != 4) continue;
            mat->SetVec4(it.key(),
                math::Vec4(it.value()[0].get<float>(), it.value()[1].get<float>(),
                           it.value()[2].get<float>(), it.value()[3].get<float>()));
        }
    }
    if (j.contains("keywords") && j["keywords"].is_object()) {
        for (auto it = j["keywords"].begin(); it != j["keywords"].end(); ++it)
            if (it.value().is_boolean()) mat->SetKeyword(it.key(), it.value().get<bool>());
    }

    mat->SetAssetId(id);
    manager.Cache().Store(id, mat, "Material");
    return id;
}

bool MaterialLoader::SaveMaterial(const std::string& path, const std::shared_ptr<render::Material>& material) {
    if (!material) return false;

    nlohmann::json j;
    j["version"] = 1;
    j["shader"] = ""; // Shader path not stored on Material; set when editing .mat manually if needed.

    j["albedo"] = nlohmann::json::array({ material->GetAlbedo().x, material->GetAlbedo().y, material->GetAlbedo().z });
    j["metallic"] = material->GetMetallic();
    j["roughness"] = material->GetRoughness();
    j["emission"] = nlohmann::json::array({ material->GetEmission().x, material->GetEmission().y, material->GetEmission().z });
    j["ao"] = material->GetAO();
    j["alpha"] = material->GetAlpha();

    nlohmann::json texObj;
    material->ForEachTexture([&texObj](const std::string& slot, const std::shared_ptr<render::Texture2D>& t) {
        if (t && t->HasAssetId())
            texObj[slot] = "assetId:" + t->GetAssetId().ToString();
    });
    j["textures"] = texObj;

    nlohmann::json floatsObj, vec3sObj, vec4sObj;
    material->ForEachFloat([&floatsObj](const std::string& k, float v) { floatsObj[k] = v; });
    material->ForEachVec3([&vec3sObj](const std::string& k, const math::Vec3& v) {
        vec3sObj[k] = nlohmann::json::array({ v.x, v.y, v.z });
    });
    material->ForEachVec4([&vec4sObj](const std::string& k, const math::Vec4& v) {
        vec4sObj[k] = nlohmann::json::array({ v.x, v.y, v.z, v.w });
    });
    j["floats"] = floatsObj;
    j["vec3s"] = vec3sObj;
    j["vec4s"] = vec4sObj;
    j["keywords"] = nlohmann::json::object(); // Material has no ForEachKeyword; persist via editor if needed.

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return f.good();
}

} // namespace assets
} // namespace eyelib
