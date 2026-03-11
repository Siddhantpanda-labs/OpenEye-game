#pragma once

struct aiScene;

namespace asset_pipe {

/** Loads a source scene file (FBX, GLTF, etc.) via Assimp. Keeps ownership of loaded data. */
class SceneLoader {
public:
    SceneLoader() = default;
    ~SceneLoader();

    /** Load from path. Returns nullptr on failure. */
    const aiScene* Load(const char* path);

    /** Last error message if Load failed. */
    const char* GetError() const { return m_error; }

private:
    struct Impl;
    Impl* m_impl = nullptr;
    static constexpr size_t kErrorLen = 512;
    char m_error[kErrorLen] = {};
};

} // namespace asset_pipe
