#include "RendererGPUResourceCache.h"
#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/AssetTypes.h"
#include "eyelib/render/MaterialDescriptor.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/rhi/RHI.h"
#include <cassert>

namespace openeye::player {

RendererGPUResourceCache::RendererGPUResourceCache(void* device, eyelib::assets::AssetManager* manager)
    : m_device(device), m_manager(manager) {
    assert(m_device && m_manager);
}

RendererGPUResourceCache::~RendererGPUResourceCache() = default;

std::shared_ptr<eyelib::render::Shader> RendererGPUResourceCache::GetShader(const eyelib::assets::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_shaders.find(id);
    if (it != m_shaders.end()) return it->second;
    const auto* desc = m_manager->GetShaderDescriptor(id);
    if (!desc) return m_manager->Registry().Cache().Get<eyelib::render::Shader>(id);
    auto sh = std::make_shared<eyelib::render::Shader>();
    if (!sh->LoadFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
        return nullptr;
    m_shaders[id] = sh;
    return sh;
}

std::shared_ptr<eyelib::render::Mesh> RendererGPUResourceCache::GetMesh(const eyelib::assets::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_meshes.find(id);
    if (it != m_meshes.end()) return it->second;
    const auto* desc = m_manager->GetMeshDescriptor(id);
    if (!desc) return m_manager->Registry().Cache().Get<eyelib::render::Mesh>(id);
    auto mesh = std::make_shared<eyelib::render::Mesh>();
    if (!mesh->CreateFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
        return nullptr;
    m_meshes[id] = mesh;
    return mesh;
}

std::shared_ptr<eyelib::render::Texture2D> RendererGPUResourceCache::GetTexture(const eyelib::assets::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_textures.find(id);
    if (it != m_textures.end()) return it->second;
    const auto* desc = m_manager->GetTextureDescriptor(id);
    if (!desc || desc->filePath.empty()) return m_manager->Registry().Cache().Get<eyelib::render::Texture2D>(id);
    auto tex = std::make_shared<eyelib::render::Texture2D>();
    if (!tex->LoadFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
        return nullptr;
    m_textures[id] = tex;
    return tex;
}

std::shared_ptr<eyelib::render::Material> RendererGPUResourceCache::GetMaterial(const eyelib::assets::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_materials.find(id);
    if (it != m_materials.end()) return it->second;
    const auto* desc = m_manager->GetMaterialDescriptor(id);
    if (!desc) return m_manager->Registry().Cache().Get<eyelib::render::Material>(id);
    auto resolveShader = [this](const std::string& name) {
        const std::string key = "name:" + name;
        eyelib::assets::AssetId sid = m_manager->Registry().GetId(key);
        if (!sid.IsValid()) sid = m_manager->Registry().LoadAssetDescriptorOnly(key, eyelib::assets::AssetType::Shader);
        return GetShader(sid);
    };
    auto resolveTexture = [this](const std::string& path) {
        eyelib::assets::AssetId tid = m_manager->Registry().GetId(path);
        if (!tid.IsValid()) tid = m_manager->Registry().LoadAssetDescriptorOnly(path, eyelib::assets::AssetType::Texture);
        return GetTexture(tid);
    };
    auto mat = eyelib::render::CreateMaterialFromDescriptor(*desc, resolveShader, resolveTexture);
    if (!mat) return nullptr;
    m_materials[id] = mat;
    return mat;
}

void RendererGPUResourceCache::OnDeviceLost() {
    m_shaders.clear();
    m_meshes.clear();
    m_textures.clear();
    m_materials.clear();
}

} // namespace openeye::player
