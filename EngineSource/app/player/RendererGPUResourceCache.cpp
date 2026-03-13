#include "RendererGPUResourceCache.h"
#include "eyelib/render/materials/MaterialDescriptor.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/materials/Material.h"
#include "eyelib/render/rhi/RHI.h"
#include <cassert>

namespace openeye::player {

RendererGPUResourceCache::RendererGPUResourceCache(void* device, eyelib::api::AssetRegistry* registry)
    : m_device(device), m_registry(registry) {
    assert(m_device && m_registry);
}

RendererGPUResourceCache::~RendererGPUResourceCache() = default;

std::shared_ptr<eyelib::render::Shader> RendererGPUResourceCache::GetShader(const eyelib::api::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_shaders.find(id);
    if (it != m_shaders.end()) return it->second;
    const auto* desc = m_registry->GetShaderDescriptor(id);
    if (!desc) return nullptr;
    auto sh = std::make_shared<eyelib::render::Shader>();
    if (!sh->LoadFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
        return nullptr;
    m_shaders[id] = sh;
    return sh;
}

std::shared_ptr<eyelib::render::Mesh> RendererGPUResourceCache::GetMesh(const eyelib::api::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_meshes.find(id);
    if (it != m_meshes.end()) return it->second;
    const auto* desc = m_registry->GetMeshDescriptor(id);
    if (!desc) return nullptr;
    auto mesh = std::make_shared<eyelib::render::Mesh>();
    if (!mesh->CreateFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
        return nullptr;
    m_meshes[id] = mesh;
    return mesh;
}

std::shared_ptr<eyelib::render::Texture2D> RendererGPUResourceCache::GetTexture(const eyelib::api::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_textures.find(id);
    if (it != m_textures.end()) return it->second;
    const auto* desc = m_registry->GetTextureDescriptor(id);
    if (!desc || desc->filePath.empty()) return nullptr;
    auto tex = std::make_shared<eyelib::render::Texture2D>();
    if (!tex->LoadFromDescriptor(static_cast<eyelib::render::rhi::IDevice*>(m_device), *desc))
        return nullptr;
    m_textures[id] = tex;
    return tex;
}

std::shared_ptr<eyelib::render::Material> RendererGPUResourceCache::GetMaterial(const eyelib::api::AssetId& id) {
    if (!id.IsValid()) return nullptr;
    auto it = m_materials.find(id);
    if (it != m_materials.end()) return it->second;
    const auto* desc = m_registry->GetMaterialDescriptor(id);
    if (!desc) return nullptr;
    auto resolveShader = [this](const std::string& name) {
        eyelib::api::AssetId sid = m_registry->GetId("name:" + name);
        if (!sid.IsValid()) sid = m_registry->LoadAssetDescriptorOnly("name:" + name, eyelib::api::AssetType::Shader);
        return GetShader(sid);
    };
    auto resolveTexture = [this](const std::string& path) {
        eyelib::api::AssetId tid = m_registry->GetId(path);
        if (!tid.IsValid()) tid = m_registry->LoadAssetDescriptorOnly(path, eyelib::api::AssetType::Texture);
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
