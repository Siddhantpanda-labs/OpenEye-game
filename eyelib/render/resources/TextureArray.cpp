#include "eyelib/render/resources/TextureArray.h"
#include "eyelib/core/Log.h"
#include <glad/glad.h>

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
        case TextureFormat::None: return GL_RGBA8;
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
        case TextureFormat::None: return GL_RGBA;
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
        case TextureFormat::None: return GL_UNSIGNED_BYTE;
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

TextureArray::TextureArray() = default;

TextureArray::~TextureArray() {
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }
}

TextureArray::TextureArray(TextureArray&& other) noexcept
    : m_id(other.m_id), m_width(other.m_width), m_height(other.m_height), m_layers(other.m_layers), m_format(other.m_format) {
    other.m_id = 0;
}

TextureArray& TextureArray::operator=(TextureArray&& other) noexcept {
    if (this != &other) {
        if (m_id) glDeleteTextures(1, &m_id);
        m_id = other.m_id; m_width = other.m_width; m_height = other.m_height; m_layers = other.m_layers; m_format = other.m_format;
        other.m_id = 0;
    }
    return *this;
}

bool TextureArray::Create(int width, int height, int layers, TextureFormat format, const void* data) {
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }
    if (width <= 0 || height <= 0 || layers <= 0) return false;
    m_width = width; m_height = height; m_layers = layers; m_format = format;
    bool srgb = (format == TextureFormat::RGB8 || format == TextureFormat::RGBA8);

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, ToGL(format, srgb), width, height, layers, 0,
                 ToGLDataFormat(format), ToGLDataType(format), data);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return true;
}

void TextureArray::SetData(const void* data, int layer, int x, int y, int width, int height) {
    if (!m_id || !data) return;
    if (width <= 0) width = m_width;
    if (height <= 0) height = m_height;
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, x, y, layer, width, height, 1,
                    ToGLDataFormat(m_format), ToGLDataType(m_format), data);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void TextureArray::Bind(int slot) const {
    if (m_id) { glActiveTexture(GL_TEXTURE0 + slot); glBindTexture(GL_TEXTURE_2D_ARRAY, m_id); }
}

void TextureArray::GenerateMipmaps() {
    if (m_id) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);
        glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
}

void TextureArray::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, ToGL(minFilter));
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, ToGL(magFilter));
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

void TextureArray::SetWrap(TextureWrap s, TextureWrap t) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_id);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, ToGL(s));
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, ToGL(t));
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

} // namespace render
} // namespace eyelib
