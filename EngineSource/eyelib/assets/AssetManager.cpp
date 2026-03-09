#include "eyelib/assets/AssetManager.h"
#include "eyelib/assets/AssetCache.h"
#include "eyelib/assets/AssetTypes.h"
#include "eyelib/render/ShaderDescriptor.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/TextureDescriptor.h"
#include "eyelib/render/MaterialDescriptor.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/assets/loaders/ShaderLoader.h"
#include "eyelib/assets/loaders/TextureLoader.h"
#include <cassert>
#include <functional>
#include <unordered_map>

namespace eyelib {
namespace assets {

namespace {

struct DeviceKey {
    AssetId id;
    void* device = nullptr;
    bool operator==(const DeviceKey& o) const { return id == o.id && device == o.device; }
};
struct DeviceKeyHash {
    size_t operator()(const DeviceKey& k) const {
        return AssetId::Hash()(k.id) ^ (std::hash<void*>()(k.device) << 1);
    }
};

} // namespace

struct AssetManager::Impl {
    std::unordered_map<AssetId, ::eyelib::render::ShaderDescriptor, AssetId::Hash> shaderDescriptors;
    std::unordered_map<AssetId, ::eyelib::render::MeshDescriptor, AssetId::Hash> meshDescriptors;
    std::unordered_map<AssetId, ::eyelib::render::TextureDescriptor, AssetId::Hash> textureDescriptors;
    std::unordered_map<AssetId, ::eyelib::render::MaterialDescriptor, AssetId::Hash> materialDescriptors;
    std::unordered_map<DeviceKey, std::shared_ptr<::eyelib::render::Shader>, DeviceKeyHash> shaderInstances;
    std::unordered_map<DeviceKey, std::shared_ptr<::eyelib::render::Mesh>, DeviceKeyHash> meshInstances;
    std::unordered_map<DeviceKey, std::shared_ptr<::eyelib::render::Texture2D>, DeviceKeyHash> textureInstances;
    std::unordered_map<DeviceKey, std::shared_ptr<::eyelib::render::Material>, DeviceKeyHash> materialInstances;
};

AssetManager::AssetManager() : m_impl(std::make_unique<Impl>()) {}

AssetManager::~AssetManager() = default;

AssetManager& AssetManager::Get() {
    static AssetManager instance;
    return instance;
}

void AssetManager::SetRenderDevice(void* device) {
    m_renderDevice = device;
    m_readyForResolution = (device != nullptr);
}

void AssetManager::StoreShaderDescriptor(const AssetId& id, const ::eyelib::render::ShaderDescriptor& desc) {
    m_impl->shaderDescriptors[id] = desc;
}

void AssetManager::StoreMeshDescriptor(const AssetId& id, const ::eyelib::render::MeshDescriptor& desc) {
    m_impl->meshDescriptors[id] = desc;
}

void AssetManager::StoreTextureDescriptor(const AssetId& id, const ::eyelib::render::TextureDescriptor& desc) {
    m_impl->textureDescriptors[id] = desc;
}

void AssetManager::StoreMaterialDescriptor(const AssetId& id, const ::eyelib::render::MaterialDescriptor& desc) {
    m_impl->materialDescriptors[id] = desc;
}

void AssetManager::RegisterShaderInstance(const AssetId& id, void* device, std::shared_ptr<::eyelib::render::Shader> instance) {
    if (id.IsValid() && device && instance)
        m_impl->shaderInstances[{id, device}] = std::move(instance);
}

void AssetManager::RegisterMeshInstance(const AssetId& id, void* device, std::shared_ptr<::eyelib::render::Mesh> instance) {
    if (id.IsValid() && device && instance)
        m_impl->meshInstances[{id, device}] = std::move(instance);
}

void AssetManager::RegisterTextureInstance(const AssetId& id, void* device, std::shared_ptr<::eyelib::render::Texture2D> instance) {
    if (id.IsValid() && device && instance)
        m_impl->textureInstances[{id, device}] = std::move(instance);
}

void AssetManager::RegisterMaterialInstance(const AssetId& id, void* device, std::shared_ptr<::eyelib::render::Material> instance) {
    if (id.IsValid() && device && instance)
        m_impl->materialInstances[{id, device}] = std::move(instance);
}

const ::eyelib::render::ShaderDescriptor* AssetManager::GetShaderDescriptor(const AssetId& id) const {
    auto it = m_impl->shaderDescriptors.find(id);
    return it != m_impl->shaderDescriptors.end() ? &it->second : nullptr;
}

const ::eyelib::render::MeshDescriptor* AssetManager::GetMeshDescriptor(const AssetId& id) const {
    auto it = m_impl->meshDescriptors.find(id);
    return it != m_impl->meshDescriptors.end() ? &it->second : nullptr;
}

const ::eyelib::render::TextureDescriptor* AssetManager::GetTextureDescriptor(const AssetId& id) const {
    auto it = m_impl->textureDescriptors.find(id);
    return it != m_impl->textureDescriptors.end() ? &it->second : nullptr;
}

const ::eyelib::render::MaterialDescriptor* AssetManager::GetMaterialDescriptor(const AssetId& id) const {
    auto it = m_impl->materialDescriptors.find(id);
    return it != m_impl->materialDescriptors.end() ? &it->second : nullptr;
}

void AssetManager::SetGPUResolver(IGPUResourceResolver* resolver) {
    m_resolver = resolver;
}

std::shared_ptr<::eyelib::render::Shader> AssetManager::GetShader(const AssetId& id) {
    if (m_resolver)
        return m_resolver->GetShader(id);
    using namespace ::eyelib::render;
    assert(m_readyForResolution && m_renderDevice && "Registry not ready for GPU resolution. Call SetRenderDevice(device) before BindAssets/BuildScene.");
    void* device = m_renderDevice;
    DeviceKey key{id, device};
    auto it = m_impl->shaderInstances.find(key);
    if (it != m_impl->shaderInstances.end()) return it->second;
    auto descIt = m_impl->shaderDescriptors.find(id);
    if (descIt != m_impl->shaderDescriptors.end()) {
        auto sh = std::make_shared<Shader>();
        if (sh->LoadFromDescriptor(static_cast<rhi::IDevice*>(device), descIt->second)) {
            m_impl->shaderInstances[key] = sh;
            return sh;
        }
    }
    return m_registry.Cache().Get<Shader>(id);
}

std::shared_ptr<::eyelib::render::Mesh> AssetManager::GetMesh(const AssetId& id) {
    if (m_resolver)
        return m_resolver->GetMesh(id);
    using namespace ::eyelib::render;
    assert(m_readyForResolution && m_renderDevice && "Registry not ready for GPU resolution. Call SetRenderDevice(device) before BindAssets/BuildScene.");
    void* device = m_renderDevice;
    DeviceKey key{id, device};
    auto it = m_impl->meshInstances.find(key);
    if (it != m_impl->meshInstances.end()) return it->second;
    auto descIt = m_impl->meshDescriptors.find(id);
    if (descIt != m_impl->meshDescriptors.end()) {
        auto mesh = std::make_shared<Mesh>();
        if (mesh->CreateFromDescriptor(static_cast<rhi::IDevice*>(device), descIt->second)) {
            m_impl->meshInstances[key] = mesh;
            return mesh;
        }
    }
    return m_registry.Cache().Get<Mesh>(id);
}

std::shared_ptr<::eyelib::render::Texture2D> AssetManager::GetTexture(const AssetId& id) {
    if (m_resolver)
        return m_resolver->GetTexture(id);
    using namespace ::eyelib::render;
    assert(m_readyForResolution && m_renderDevice && "Registry not ready for GPU resolution. Call SetRenderDevice(device) before BindAssets/BuildScene.");
    void* device = m_renderDevice;
    DeviceKey key{id, device};
    auto it = m_impl->textureInstances.find(key);
    if (it != m_impl->textureInstances.end()) return it->second;
    auto descIt = m_impl->textureDescriptors.find(id);
    if (descIt != m_impl->textureDescriptors.end() && !descIt->second.filePath.empty()) {
        auto tex = std::make_shared<Texture2D>();
        if (tex->LoadFromDescriptor(static_cast<rhi::IDevice*>(device), descIt->second)) {
            m_impl->textureInstances[key] = tex;
            return tex;
        }
    }
    return m_registry.Cache().Get<Texture2D>(id);
}

std::shared_ptr<::eyelib::render::Material> AssetManager::GetMaterial(const AssetId& id) {
    if (m_resolver)
        return m_resolver->GetMaterial(id);
    using namespace ::eyelib::render;
    assert(m_readyForResolution && m_renderDevice && "Registry not ready for GPU resolution. Call SetRenderDevice(device) before BindAssets/BuildScene.");
    void* device = m_renderDevice;
    DeviceKey key{id, device};
    auto it = m_impl->materialInstances.find(key);
    if (it != m_impl->materialInstances.end()) return it->second;
    auto descIt = m_impl->materialDescriptors.find(id);
    if (descIt != m_impl->materialDescriptors.end()) {
        auto resolveShader = [this](const std::string& name) {
            const std::string key = "name:" + name;
            AssetId sid = m_registry.GetId(key);
            if (!sid.IsValid()) sid = m_registry.LoadAssetDescriptorOnly(key, AssetType::Shader);
            return GetShader(sid);
        };
        auto resolveTexture = [this](const std::string& path) {
            AssetId tid = m_registry.GetId(path);
            if (!tid.IsValid()) tid = m_registry.LoadAssetDescriptorOnly(path, AssetType::Texture);
            return GetTexture(tid);
        };
        auto mat = CreateMaterialFromDescriptor(descIt->second, resolveShader, resolveTexture);
        if (mat) {
            m_impl->materialInstances[key] = mat;
            return mat;
        }
    }
    return m_registry.Cache().Get<Material>(id);
}

void AssetManager::ClearDescriptorsAndInstances() {
    m_readyForResolution = false;
    m_impl->shaderDescriptors.clear();
    m_impl->meshDescriptors.clear();
    m_impl->textureDescriptors.clear();
    m_impl->materialDescriptors.clear();
    m_impl->shaderInstances.clear();
    m_impl->meshInstances.clear();
    m_impl->textureInstances.clear();
    m_impl->materialInstances.clear();
}

void AssetManager::OnDeviceLost() {
    m_readyForResolution = false;
    if (m_resolver)
        m_resolver->OnDeviceLost();
    m_impl->shaderInstances.clear();
    m_impl->meshInstances.clear();
    m_impl->textureInstances.clear();
    m_impl->materialInstances.clear();
}

} // namespace assets
} // namespace eyelib
