#include "LightGizmoIcons.h"
#include "api/platform/All.h"

#include "eyelib/world/components/Light.h"

#include <glad/glad.h>
#include <cstring>
#include <string>

// Use stb_image from EyelibRender (Texture2D.cpp); do not define STB_IMAGE_IMPLEMENTATION here.
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

bool LightGizmoIcons::LoadPngToGlTexture(const char* path, uint32_t* outTexId) {
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

bool LightGizmoIcons::TryLoadFromDir(const char* dir) {
    if (!dir) return false;
    const char* names[] = {
        "DirectionalLight Gizmo.png",
        "PointLight Gizmo.png",
        "SpotLight Gizmo.png"
    };
    uint32_t* ids[] = { &m_directionalTexId, &m_pointTexId, &m_spotTexId };
    for (int i = 0; i < 3; ++i) {
        std::string path = JoinPath(dir, names[i]);
        if (!LoadPngToGlTexture(path.c_str(), ids[i]))
            return false;
    }
    return true;
}

bool LightGizmoIcons::EnsureLoaded() {
    if (m_directionalTexId && m_pointTexId && m_spotTexId) return true;
    if (m_attempted) return false;
    m_attempted = true;

    std::string exeDir = api::platform::filesystem::GetDirectory(api::platform::filesystem::GetExecutablePath());
    if (exeDir.empty()) return false;

    const char* candidates[] = {
        "ui/icon",
        "app/editor/ui/icon",
        "../../../app/editor/ui/icon",
        "../../app/editor/ui/icon",
    };
    for (const char* sub : candidates) {
        std::string dir = JoinPath(exeDir, sub);
        if (TryLoadFromDir(dir.c_str())) return true;
    }
    return false;
}

uint64_t LightGizmoIcons::GetTexture(::eyelib::world::components::Light::Type type) const {
    using T = ::eyelib::world::components::Light::Type;
    switch (type) {
        case T::Directional: return static_cast<uint64_t>(m_directionalTexId);
        case T::Point:       return static_cast<uint64_t>(m_pointTexId);
        case T::Spot:        return static_cast<uint64_t>(m_spotTexId);
        default:             return 0;
    }
}

void LightGizmoIcons::Shutdown() {
    if (m_directionalTexId) { glDeleteTextures(1, &m_directionalTexId); m_directionalTexId = 0; }
    if (m_pointTexId)       { glDeleteTextures(1, &m_pointTexId);       m_pointTexId = 0; }
    if (m_spotTexId)        { glDeleteTextures(1, &m_spotTexId);        m_spotTexId = 0; }
    m_attempted = false;
}

} // namespace openeye::editor
