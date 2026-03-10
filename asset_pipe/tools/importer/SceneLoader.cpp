#include "asset_pipe/tools/importer/SceneLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <cstring>

namespace asset_pipe {

struct SceneLoader::Impl {
    Assimp::Importer importer;
};

SceneLoader::~SceneLoader() {
    delete m_impl;
    m_impl = nullptr;
}

const aiScene* SceneLoader::Load(const char* path) {
    if (!path || !path[0]) {
        std::strncpy(m_error, "Empty path", kErrorLen - 1);
        m_error[kErrorLen - 1] = '\0';
        return nullptr;
    }

    if (!m_impl) m_impl = new Impl();

    m_impl->importer.FreeScene();
    m_error[0] = '\0';

    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes;

    const aiScene* scene = m_impl->importer.ReadFile(path, flags);
    if (!scene) {
        std::strncpy(m_error, m_impl->importer.GetErrorString(), kErrorLen - 1);
        m_error[kErrorLen - 1] = '\0';
        return nullptr;
    }
    return scene;
}

} // namespace asset_pipe
