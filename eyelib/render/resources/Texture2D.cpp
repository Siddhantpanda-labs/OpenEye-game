#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glad/glad.h>
#include <algorithm>
#include <cstring>

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
        case TextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
        case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
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
        case TextureFormat::Depth24: return GL_DEPTH_COMPONENT;
        case TextureFormat::Depth24Stencil8: return GL_DEPTH_STENCIL;
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
        case TextureFormat::Depth24: return GL_UNSIGNED_INT;
        case TextureFormat::Depth24Stencil8: return GL_UNSIGNED_INT_24_8;
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

Texture2D::Texture2D() = default;

Texture2D::~Texture2D() {
    if (m_id) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : m_id(other.m_id), m_width(other.m_width), m_height(other.m_height), m_format(other.m_format) {
    other.m_id = 0;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        if (m_id) glDeleteTextures(1, &m_id);
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        other.m_id = 0;
    }
    return *this;
}

bool Texture2D::Create(int width, int height, TextureFormat format, const void* data) {
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }
    if (width <= 0 || height <= 0) return false;

    m_width = width;
    m_height = height;
    m_format = format;

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, ToGL(format, false), width, height, 0,
                 ToGLDataFormat(format), ToGLDataType(format), data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool Texture2D::LoadFromFile(const std::string& path, bool isSRGB, bool generateMipmaps) {
    std::vector<uint8_t> bytes = eyelib::platform::filesystem::ReadFile(path);
    if (bytes.empty()) {
        core::Log::ErrorF("Texture2D: failed to read file: %s", path.c_str());
        return false;
    }
    return LoadFromMemory(bytes.data(), bytes.size(), isSRGB, generateMipmaps);
}

bool Texture2D::LoadFromMemory(const uint8_t* data, size_t size, bool isSRGB, bool generateMipmaps) {
    if (!data || size == 0) return false;
    if (m_id) { glDeleteTextures(1, &m_id); m_id = 0; }

    stbi_set_flip_vertically_on_load(0);
    int w = 0, h = 0, ch = 0;
    unsigned char* decoded = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &ch, 0);
    if (!decoded) {
        core::Log::ErrorF("Texture2D: stb_image failed: %s", stbi_failure_reason());
        return false;
    }

    TextureFormat fmt = TextureFormat::RGBA8;
    if (ch == 1) fmt = TextureFormat::R8;
    else if (ch == 2) fmt = TextureFormat::RG8;
    else if (ch == 3) fmt = TextureFormat::RGB8;

    m_width = w;
    m_height = h;
    m_format = fmt;

    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexImage2D(GL_TEXTURE_2D, 0, ToGL(fmt, isSRGB), w, h, 0,
                 ToGLDataFormat(fmt), ToGLDataType(fmt), decoded);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLint minF = generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (generateMipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(decoded);
    return true;
}

void Texture2D::SetData(const void* data, int x, int y, int width, int height) {
    if (!m_id || !data) return;
    if (width <= 0) width = m_width;
    if (height <= 0) height = m_height;
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, ToGLDataFormat(m_format), ToGLDataType(m_format), data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::vector<uint8_t> Texture2D::GetData() const {
    if (!m_id) return {};
    size_t bpp = 4;
    if (m_format == TextureFormat::R8) bpp = 1;
    else if (m_format == TextureFormat::RG8) bpp = 2;
    else if (m_format == TextureFormat::RGB8) bpp = 3;
    std::vector<uint8_t> out(static_cast<size_t>(m_width * m_height) * bpp);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glGetTexImage(GL_TEXTURE_2D, 0, ToGLDataFormat(m_format), ToGLDataType(m_format), out.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    return out;
}

void Texture2D::Bind(int slot) const {
    if (m_id) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_id);
    }
}

void Texture2D::GenerateMipmaps() {
    if (m_id) {
        glBindTexture(GL_TEXTURE_2D, m_id);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Texture2D::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ToGL(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ToGL(magFilter));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::SetWrap(TextureWrap s, TextureWrap t) {
    if (!m_id) return;
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ToGL(s));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ToGL(t));
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace render
} // namespace eyelib
