#include "eyelib/assets/StreamingManager.h"

#include "eyelib/assets/loaders/AnimationLoader.h"
#include "eyelib/assets/loaders/AudioLoader.h"
#include "eyelib/assets/loaders/MeshLoader.h"
#include "eyelib/assets/loaders/ShaderLoader.h"
#include "eyelib/assets/loaders/TextureLoader.h"
#include "eyelib/core/Log.h"
#include "eyelib/assets/AssetManager.h"

#include <algorithm>
#include <cctype>
#include <thread>

namespace eyelib {
namespace assets {

namespace {
std::string Lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}
} // namespace

AssetId StreamingManager::RequestAsset(const std::string& path, int priority) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_requests.find(path);
    if (it != m_requests.end()) return it->second->id;

    auto req = std::make_shared<Request>();
    req->priority = priority;
    m_requests[path] = req;

    std::thread([this, path, req]() {
        AssetId id = LoadByExtension(path);
        if (!req->cancelled.load()) {
            req->id = id;
            req->progress.store(1.0f);
            const size_t usage = AssetManager::Get().Registry().GetAssetMemoryUsage();
            if (usage > m_memoryBudget) {
                core::Log::WarnF("StreamingManager: memory budget exceeded (%zu / %zu)", usage, m_memoryBudget);
            }
        }
    }).detach();

    return AssetId::Invalid();
}

void StreamingManager::CancelRequest(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_requests.find(path);
    if (it != m_requests.end()) {
        it->second->cancelled.store(true);
        it->second->progress.store(0.0f);
    }
}

float StreamingManager::GetLoadProgress(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_requests.find(path);
    if (it == m_requests.end()) return 0.0f;
    return it->second->progress.load();
}

bool StreamingManager::IsLoaded(const std::string& path) const {
    const AssetId id = AssetManager::Get().Registry().GetId(path);
    if (!id.IsValid()) return false;
    return AssetManager::Get().Cache().Has(id);
}

AssetId StreamingManager::LoadByExtension(const std::string& path) {
    const std::string lower = Lower(path);
    if (lower.rfind(".png") != std::string::npos || lower.rfind(".jpg") != std::string::npos ||
        lower.rfind(".jpeg") != std::string::npos || lower.rfind(".hdr") != std::string::npos) {
        return TextureLoader::LoadTexture(path, true);
    }
    if (lower.rfind(".obj") != std::string::npos || lower.rfind(".fbx") != std::string::npos ||
        lower.rfind(".gltf") != std::string::npos || lower.rfind(".dae") != std::string::npos) {
        return MeshLoader::LoadMesh(path);
    }
    if (lower.rfind(".wav") != std::string::npos || lower.rfind(".ogg") != std::string::npos ||
        lower.rfind(".mp3") != std::string::npos) {
        return AudioLoader::LoadAudio(path);
    }
    if (lower.rfind(".vert") != std::string::npos) {
        const std::string frag = path.substr(0, path.size() - 5) + ".frag";
        return ShaderLoader::LoadShader(path, frag);
    }
    if (lower.rfind(".frag") != std::string::npos) {
        const std::string vert = path.substr(0, path.size() - 5) + ".vert";
        return ShaderLoader::LoadShader(vert, path);
    }
    if (lower.rfind(".anim") != std::string::npos || lower.rfind(".fbx") != std::string::npos ||
        lower.rfind(".gltf") != std::string::npos || lower.rfind(".dae") != std::string::npos) {
        return AnimationLoader::LoadAnimation(path);
    }

    core::Log::WarnF("StreamingManager: unknown asset type for %s", path.c_str());
    return AssetId::Invalid();
}

} // namespace assets
} // namespace eyelib
