#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#include <stb_image.h>
#include <glad/glad.h>

namespace eyelib {
namespace render {

namespace {
static GLint ToGL(TextureFilter f) {
    switch (f) {
        case TextureFilter::Nearest: return GL_NEAREST;
        case TextureFilter::Linear: return GL_LINEAR;
        case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
        case TextureFilter::LinearMipmapNearest: return GL_LINEAR_MIPMAP_NEAREST;
        case TextureFilter::NearestMipmapLinear: return GL_NEAREST_MIPMAP_LINEAR;
        case TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
        default: return GL_LINEAR;
    }
}
static GLint ToGL(TextureWrap w) {
    switch (w) {
        case TextureWrap::Repeat: return GL_REPEAT;
        case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
        default: return GL_REPEAT;
    }
}
} // namespace

TextureCube::TextureCube() = default;

TextureCube::~TextureCube() {
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }
}

TextureCube::TextureCube(TextureCube&& other) noexcept : m_id(other.m_id), m_size(other.m_size) {
    other.m_id = 0;
}

TextureCube& TextureCube::operator=(TextureCube&& other) noexcept {
    if (this != &other) {
        if (m_id) glDeleteTextures(1, &m_id);
        m_id = other.m_id; m_size = other.m_size;
        other.m_id = 0;
    }
    return *this;
}

bool TextureCube::LoadFromFiles(const std::string& px, const std::string& nx,
                                const std::string& py, const std::string& ny,
                                const std::string& pz, const std::string& nz,
                                bool isSRGB, bool generateMipmaps) {
    std::vector<std::string> paths = { px, nx, py, ny, pz, nz };
    return LoadFromFiles(paths, isSRGB, generateMipmaps);
}

bool TextureCube::LoadFromFiles(const std::vector<std::string>& facePaths, bool isSRGB, bool generateMipmaps) {
    if (facePaths.size() != 6) {
        core::Log::Error("TextureCube: exactly 6 face paths required");
        return false;
    }
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }

    stbi_set_flip_vertically_on_load(0);
    int width = 0, height = 0, channels = 0;
    unsigned char* datas[6] = {};

    for (int i = 0; i < 6; ++i) {
        std::vector<uint8_t> bytes = eyelib::platform::filesystem::ReadFile(facePaths[static_cast<size_t>(i)]);
        if (bytes.empty()) {
            core::Log::ErrorF("TextureCube: failed to read %s", facePaths[static_cast<size_t>(i)].c_str());
            for (int j = 0; j < i; ++j) stbi_image_free(datas[j]);
            return false;
        }
        int w, h, c;
        datas[i] = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &c, 0);
        if (!datas[i]) {
            core::Log::ErrorF("TextureCube: stb_image failed for face %d", i);
            for (int j = 0; j < i; ++j) stbi_image_free(datas[j]);
            return false;
        }
        if (i == 0) { width = w; height = h; channels = c; }
        else if (w != width || h != height || c != channels) {
            core::Log::Error("TextureCube: all faces must have same dimensions and channels");
            for (int j = 0; j <= i; ++j) stbi_image_free(datas[j]);
            return false;
        }
    }

    m_size = width;
    GLenum df = (channels == 1) ? GL_RED : (channels == 3) ? GL_RGB : GL_RGBA;
    GLenum internal = df;
    if (isSRGB && (channels == 3 || channels == 4))
        internal = (channels == 3) ? GL_SRGB8 : GL_SRGB8_ALPHA8;

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal, width, height, 0, df, GL_UNSIGNED_BYTE, datas[i]);
        stbi_image_free(datas[i]);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    GLint minF = generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, minF);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (generateMipmaps) glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return true;
}

void TextureCube::Bind(int slot) const {
    if (m_id) { glActiveTexture(GL_TEXTURE0 + slot); glBindTexture(GL_TEXTURE_CUBE_MAP, m_id); }
}

void TextureCube::GenerateMipmaps() {
    if (m_id) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }
}

void TextureCube::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, ToGL(minFilter));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, ToGL(magFilter));
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void TextureCube::SetWrap(TextureWrap s, TextureWrap t, TextureWrap r) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_id);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, ToGL(s));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, ToGL(t));
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, ToGL(r));
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} // namespace render
} // namespace eyelib
