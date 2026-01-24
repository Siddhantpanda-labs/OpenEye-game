#include "eyelib/render/resources/Texture3D.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>
#include <algorithm>

namespace eyelib {
namespace render {

namespace {
static GLenum ToGL(TextureFormat f, bool isSRGB) {
    switch (f) {
        case TextureFormat::R8: return GL_R8;
        case TextureFormat::RG8: return GL_RG8;
        case TextureFormat::RGB8: return isSRGB ? GL_SRGB8 : GL_RGB8;
        case TextureFormat::RGBA8: return isSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        case TextureFormat::RGB16F: return GL_RGB16F;
        case TextureFormat::RGBA16F: return GL_RGBA16F;
        default: return GL_RGBA8;
    }
}
static GLenum ToGLDataFormat(TextureFormat f) {
    switch (f) {
        case TextureFormat::R8: return GL_RED;
        case TextureFormat::RG8: return GL_RG;
        case TextureFormat::RGB8: return GL_RGB;
        case TextureFormat::RGBA8: return GL_RGBA;
        case TextureFormat::RGB16F: return GL_RGB;
        case TextureFormat::RGBA16F: return GL_RGBA;
        default: return GL_RGBA;
    }
}
static GLenum ToGLDataType(TextureFormat f) {
    switch (f) {
        case TextureFormat::RGB8:
        case TextureFormat::RGBA8:
        case TextureFormat::R8:
        case TextureFormat::RG8:
            return GL_UNSIGNED_BYTE;
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F:
            return GL_FLOAT;
        default: return GL_UNSIGNED_BYTE;
    }
}
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

Texture3D::Texture3D() = default;

Texture3D::~Texture3D() {
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }
}

Texture3D::Texture3D(Texture3D&& other) noexcept
    : m_id(other.m_id), m_width(other.m_width), m_height(other.m_height), m_depth(other.m_depth), m_format(other.m_format) {
    other.m_id = 0;
}

Texture3D& Texture3D::operator=(Texture3D&& other) noexcept {
    if (this != &other) {
        if (m_id) glDeleteTextures(1, &m_id);
        m_id = other.m_id; m_width = other.m_width; m_height = other.m_height; m_depth = other.m_depth; m_format = other.m_format;
        other.m_id = 0;
    }
    return *this;
}

bool Texture3D::Create(int width, int height, int depth, TextureFormat format, const void* data) {
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }
    if (width <= 0 || height <= 0 || depth <= 0) return false;
    m_width = width; m_height = height; m_depth = depth; m_format = format;
    bool srgb = (format == TextureFormat::RGB8 || format == TextureFormat::RGBA8);

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_3D, m_id);
    glTexImage3D(GL_TEXTURE_3D, 0, ToGL(format, srgb), width, height, depth, 0,
                 ToGLDataFormat(format), ToGLDataType(format), data);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_3D, 0);
    return true;
}

void Texture3D::SetData(const void* data, int x, int y, int z, int width, int height, int depth) {
    if (!m_id || !data) return;
    if (width <= 0) width = m_width;
    if (height <= 0) height = m_height;
    if (depth <= 0) depth = m_depth;
    glBindTexture(GL_TEXTURE_3D, m_id);
    glTexSubImage3D(GL_TEXTURE_3D, 0, x, y, z, width, height, depth, ToGLDataFormat(m_format), ToGLDataType(m_format), data);
    glBindTexture(GL_TEXTURE_3D, 0);
}

std::vector<uint8_t> Texture3D::GetData() const {
    if (!m_id) return {};
    size_t bpp = 4;
    if (m_format == TextureFormat::R8) bpp = 1;
    else if (m_format == TextureFormat::RG8) bpp = 2;
    else if (m_format == TextureFormat::RGB8) bpp = 3;
    std::vector<uint8_t> out(static_cast<size_t>(m_width * m_height * m_depth) * bpp);
    glBindTexture(GL_TEXTURE_3D, m_id);
    glGetTexImage(GL_TEXTURE_3D, 0, ToGLDataFormat(m_format), ToGLDataType(m_format), out.data());
    glBindTexture(GL_TEXTURE_3D, 0);
    return out;
}

void Texture3D::Bind(int slot) const {
    if (m_id) { glActiveTexture(GL_TEXTURE0 + slot); glBindTexture(GL_TEXTURE_3D, m_id); }
}

void Texture3D::GenerateMipmaps() {
    if (m_id) { glBindTexture(GL_TEXTURE_3D, m_id); glGenerateMipmap(GL_TEXTURE_3D); glBindTexture(GL_TEXTURE_3D, 0); }
}

void Texture3D::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_3D, m_id);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, ToGL(minFilter));
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, ToGL(magFilter));
    glBindTexture(GL_TEXTURE_3D, 0);
}

void Texture3D::SetWrap(TextureWrap s, TextureWrap t, TextureWrap r) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_3D, m_id);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, ToGL(s));
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, ToGL(t));
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, ToGL(r));
    glBindTexture(GL_TEXTURE_3D, 0);
}

} // namespace render
} // namespace eyelib
