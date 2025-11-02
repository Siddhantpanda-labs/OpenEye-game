#include "Cubemap.h"
#include "core/log/Log.h"
#include <stb_image.h>
#include <vector>

namespace OpenEye {
namespace Rendering {

Cubemap::~Cubemap() {
    if (m_textureID) {
        glDeleteTextures(1, &m_textureID);
    }
}

Cubemap::Cubemap(Cubemap&& other) noexcept {
    m_textureID = other.m_textureID;
    other.m_textureID = 0;
}

Cubemap& Cubemap::operator=(Cubemap&& other) noexcept {
    if (this != &other) {
        if (m_textureID) glDeleteTextures(1, &m_textureID);
        m_textureID = other.m_textureID;
        other.m_textureID = 0;
    }
    return *this;
}

bool Cubemap::LoadFromFiles(const std::vector<std::string>& facePaths, bool srgb) {
    if (facePaths.size() != 6) {
        OE_ERROR("Cubemap: exactly 6 face paths required");
        return false;
    }

    if (m_textureID == 0) {
        glGenTextures(1, &m_textureID);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);

    stbi_set_flip_vertically_on_load(false); // cubemaps expect no vertical flip

    int width = 0, height = 0, channels = 0;
    for (size_t i = 0; i < 6; ++i) {
        int w, h, c;
        unsigned char* data = stbi_load(facePaths[i].c_str(), &w, &h, &c, 0);
        if (!data) {
            OE_ERROR_F("Cubemap: failed to load face %zu from %s", i, facePaths[i].c_str());
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            return false;
        }
        if (i == 0) { width = w; height = h; channels = c; }
        GLenum dataFormat = (c == 1) ? GL_RED : (c == 3) ? GL_RGB : GL_RGBA;
        GLenum internal = dataFormat;
        if (srgb && (dataFormat == GL_RGB || dataFormat == GL_RGBA)) {
            internal = (dataFormat == GL_RGB) ? GL_SRGB : GL_SRGB_ALPHA;
        }
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + (GLenum)i, 0, internal, w, h, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return true;
}

bool Cubemap::CreateSolidColor(float r, float g, float b, uint32_t resolution) {
    if (m_textureID == 0) {
        glGenTextures(1, &m_textureID);
    }
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
    
    // Create solid color data for all 6 faces
    std::vector<float> colorData(resolution * resolution * 3);
    for (size_t i = 0; i < resolution * resolution * 3; i += 3) {
        colorData[i]     = r;
        colorData[i + 1] = g;
        colorData[i + 2] = b;
    }
    
    // Upload to all 6 faces
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 
                     resolution, resolution, 0, GL_RGB, GL_FLOAT, colorData.data());
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return true;
}

void Cubemap::Bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureID);
}

void Cubemap::Unbind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} // namespace Rendering
} // namespace OpenEye


