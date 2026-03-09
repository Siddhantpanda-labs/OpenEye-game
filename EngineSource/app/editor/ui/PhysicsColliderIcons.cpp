#include "PhysicsColliderIcons.h"
#include "api/platform/All.h"

#include <glad/glad.h>
#include <cstring>
#include <string>

#include "thirdparty/stb/stb_image.h"

namespace openeye::editor {

namespace {

std::string JoinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    char last = a.back();
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
}

} // namespace

bool PhysicsColliderIcons::LoadPngToGlTexture(const char* path, uint32_t* outTexId) {
    if (!path || !outTexId) return false;
    int w = 0, h = 0, comp = 0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(path, &w, &h, &comp, 4);
    if (!data || w <= 0 || h <= 0) return false;

    uint32_t texId = 0;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);
    *outTexId = texId;
    return true;
}

bool PhysicsColliderIcons::TryLoadFromDir(const char* dir) {
    if (!dir) return false;
    if (!LoadPngToGlTexture(JoinPath(dir, "d_BoxCollider Icon.png").c_str(), &m_boxTexId)) return false;
    if (!LoadPngToGlTexture(JoinPath(dir, "d_SphereCollider Icon.png").c_str(), &m_sphereTexId)) return false;
    if (!LoadPngToGlTexture(JoinPath(dir, "d_CapsuleCollider Icon.png").c_str(), &m_capsuleTexId)) return false;
    LoadPngToGlTexture(JoinPath(dir, "d_MeshCollider Icon.png").c_str(), &m_meshTexId); // optional
    LoadPngToGlTexture(JoinPath(dir, "d_RigidbodyIcon.png").c_str(), &m_rigidbodyTexId); // optional
    return true;
}

bool PhysicsColliderIcons::EnsureLoaded() {
    if (m_boxTexId && m_sphereTexId && m_capsuleTexId) return true;
    if (m_attempted) return false;
    m_attempted = true;

    std::string exeDir = api::platform::filesystem::GetDirectory(api::platform::filesystem::GetExecutablePath());
    if (exeDir.empty()) return false;

    const char* candidates[] = {
        "ui/icon/physics",
        "app/editor/ui/icon/physics",
        "../../../app/editor/ui/icon/physics",
        "../../app/editor/ui/icon/physics",
    };
    for (const char* sub : candidates) {
        std::string d = JoinPath(exeDir, sub);
        if (TryLoadFromDir(d.c_str())) return true;
    }
    return false;
}

uint64_t PhysicsColliderIcons::GetTexture(ColliderIconShape shape) const {
    switch (shape) {
        case ColliderIconShape::Box:    return static_cast<uint64_t>(m_boxTexId);
        case ColliderIconShape::Sphere: return static_cast<uint64_t>(m_sphereTexId);
        case ColliderIconShape::Capsule: return static_cast<uint64_t>(m_capsuleTexId);
        case ColliderIconShape::Mesh:   return static_cast<uint64_t>(m_meshTexId);
        default: return 0;
    }
}

uint64_t PhysicsColliderIcons::GetRigidBodyTexture() const {
    return static_cast<uint64_t>(m_rigidbodyTexId);
}

void PhysicsColliderIcons::Shutdown() {
    if (m_boxTexId)    { glDeleteTextures(1, &m_boxTexId);    m_boxTexId = 0; }
    if (m_sphereTexId) { glDeleteTextures(1, &m_sphereTexId); m_sphereTexId = 0; }
    if (m_capsuleTexId){ glDeleteTextures(1, &m_capsuleTexId); m_capsuleTexId = 0; }
    if (m_meshTexId)   { glDeleteTextures(1, &m_meshTexId);   m_meshTexId = 0; }
    if (m_rigidbodyTexId) { glDeleteTextures(1, &m_rigidbodyTexId); m_rigidbodyTexId = 0; }
    m_attempted = false;
}

} // namespace openeye::editor
