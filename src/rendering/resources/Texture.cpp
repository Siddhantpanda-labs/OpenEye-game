#include "Texture.h"
#include "core/log/Log.h"

// Define STB_IMAGE_IMPLEMENTATION before including (if not already defined)
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>

#include <algorithm>

namespace OpenEye {
namespace Rendering {

Texture::Texture() = default;

Texture::~Texture() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_textureID(other.m_textureID)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_channels(other.m_channels)
    , m_format(other.m_format)
    , m_isSRGB(other.m_isSRGB)
    , m_internalFormat(other.m_internalFormat)
    , m_dataFormat(other.m_dataFormat)
    , m_dataType(other.m_dataType)
{
    other.m_textureID = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
        }
        
        m_textureID = other.m_textureID;
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        m_format = other.m_format;
        m_isSRGB = other.m_isSRGB;
        m_internalFormat = other.m_internalFormat;
        m_dataFormat = other.m_dataFormat;
        m_dataType = other.m_dataType;
        
        other.m_textureID = 0;
    }
    return *this;
}

bool Texture::LoadFromFile(const std::string& path, bool isSRGB, bool generateMipmaps) {
    // Don't flip images - they should match the UV coordinates from the model
    // If textures appear flipped, it's usually because the model format uses different conventions
    stbi_set_flip_vertically_on_load(false);
    
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        OE_ERROR_F("Failed to load texture: %s - %s", path.c_str(), stbi_failure_reason());
        return false;
    }
    
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_isSRGB = isSRGB;
    
    // Determine format based on channels
    TextureFormat format;
    if (channels == 1) {
        format = TextureFormat::R8;
    } else if (channels == 2) {
        format = TextureFormat::RG8;
    } else if (channels == 3) {
        format = TextureFormat::RGB8;
    } else {
        format = TextureFormat::RGBA8;
    }
    
    m_format = format;
    m_internalFormat = GetGLInternalFormat(format, isSRGB);
    m_dataFormat = GetGLDataFormat(format);
    m_dataType = GetGLDataType(format);
    
    // Create texture
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, 
                 m_dataFormat, m_dataType, data);
    
    if (generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    
    // Set default parameters (these will be overridden if SetWrap/SetFilter called later)
    // Use REPEAT for UV coordinates (standard for texture atlases)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    stbi_image_free(data);
    
    OE_DEBUG_F("Loaded texture: %s (%dx%d, %d channels)", path.c_str(), m_width, m_height, m_channels);
    return true;
}

bool Texture::LoadFromMemory(const unsigned char* data, int sizeBytes, bool isSRGB, bool generateMipmaps) {
    if (!data || sizeBytes <= 0) {
        OE_ERROR("LoadFromMemory: invalid buffer");
        return false;
    }
    
    stbi_set_flip_vertically_on_load(false);
    int width = 0, height = 0, channels = 0;
    unsigned char* decoded = stbi_load_from_memory(data, sizeBytes, &width, &height, &channels, 0);
    if (!decoded) {
        OE_ERROR_F("LoadFromMemory: stbi failed - %s", stbi_failure_reason());
        return false;
    }
    
    // Determine format
    TextureFormat format = TextureFormat::RGBA8;
    if (channels == 1) format = TextureFormat::R8;
    else if (channels == 2) format = TextureFormat::RG8;
    else if (channels == 3) format = TextureFormat::RGB8;
    else format = TextureFormat::RGBA8;
    
    // Create GL texture from decoded pixels
    m_width = width;
    m_height = height;
    m_channels = channels;
    m_isSRGB = isSRGB;
    m_format = format;
    m_internalFormat = GetGLInternalFormat(format, isSRGB);
    m_dataFormat = GetGLDataFormat(format);
    m_dataType = GetGLDataType(format);

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, m_dataFormat, m_dataType, decoded);
    if (generateMipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(decoded);
    return true;
}

bool Texture::LoadFromPixelsRGBA8(int width, int height, const unsigned char* rgbaData, bool isSRGB, bool generateMipmaps) {
    if (!rgbaData || width <= 0 || height <= 0) {
        OE_ERROR("LoadFromPixelsRGBA8: invalid parameters");
        return false;
    }

    m_width = width;
    m_height = height;
    m_channels = 4;
    m_isSRGB = isSRGB;
    m_format = TextureFormat::RGBA8;
    m_internalFormat = GetGLInternalFormat(m_format, isSRGB);
    m_dataFormat = GetGLDataFormat(m_format);
    m_dataType = GetGLDataType(m_format);

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0, m_dataFormat, m_dataType, rgbaData);
    if (generateMipmaps) glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool Texture::Create(const TextureSpec& spec, const void* data) {
    m_width = spec.width;
    m_height = spec.height;
    m_format = spec.format;
    m_isSRGB = spec.isSRGB;
    m_internalFormat = GetGLInternalFormat(spec.format, spec.isSRGB);
    m_dataFormat = GetGLDataFormat(spec.format);
    m_dataType = GetGLDataType(spec.format);
    
    if (m_width <= 0 || m_height <= 0) {
        OE_ERROR("Invalid texture dimensions");
        return false;
    }
    
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, m_width, m_height, 0,
                 m_dataFormat, m_dataType, data);
    
    if (spec.generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    
    SetWrap(spec.wrapS, spec.wrapT);
    SetFilter(spec.minFilter, spec.magFilter);
    
    if (spec.anisotropy > 0.0f) {
        SetAnisotropy(spec.anisotropy);
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    return true;
}

void Texture::UpdateData(const void* data, int x, int y, int width, int height) {
    if (m_textureID == 0) return;
    
    if (width == 0) width = m_width;
    if (height == 0) height = m_height;
    
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, m_dataFormat, m_dataType, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::Bind(int unit) const {
    if (m_textureID != 0) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, m_textureID);
    }
}

void Texture::Unbind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::SetWrap(TextureWrap wrapS, TextureWrap wrapT) {
    if (m_textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapS));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapT));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    if (m_textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, m_textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void Texture::SetAnisotropy(float anisotropy) {
    if (m_textureID == 0) return;
    
    GLfloat maxAnisotropy = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
    float clamped = std::min(anisotropy, maxAnisotropy);
    
    glBindTexture(GL_TEXTURE_2D, m_textureID);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, clamped);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLenum Texture::GetGLInternalFormat(TextureFormat format, bool isSRGB) const {
    switch (format) {
        case TextureFormat::R8:
            return GL_R8;
        case TextureFormat::RG8:
            return GL_RG8;
        case TextureFormat::RGB8:
            return isSRGB ? GL_SRGB8 : GL_RGB8;
        case TextureFormat::RGBA8:
            return isSRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        case TextureFormat::RGB16F:
            return GL_RGB16F;
        case TextureFormat::RGBA16F:
            return GL_RGBA16F;
        case TextureFormat::Depth24:
            return GL_DEPTH_COMPONENT24;
        case TextureFormat::Depth24Stencil8:
            return GL_DEPTH24_STENCIL8;
        default:
            return GL_RGBA8;
    }
}

GLenum Texture::GetGLDataFormat(TextureFormat format) const {
    switch (format) {
        case TextureFormat::R8:
            return GL_RED;
        case TextureFormat::RG8:
            return GL_RG;
        case TextureFormat::RGB8:
            return GL_RGB;
        case TextureFormat::RGBA8:
            return GL_RGBA;
        case TextureFormat::RGB16F:
            return GL_RGB;
        case TextureFormat::RGBA16F:
            return GL_RGBA;
        case TextureFormat::Depth24:
            return GL_DEPTH_COMPONENT;
        case TextureFormat::Depth24Stencil8:
            return GL_DEPTH_STENCIL;
        default:
            return GL_RGBA;
    }
}

GLenum Texture::GetGLDataType(TextureFormat format) const {
    switch (format) {
        case TextureFormat::RGB8:
        case TextureFormat::RGBA8:
        case TextureFormat::R8:
        case TextureFormat::RG8:
            return GL_UNSIGNED_BYTE;
        case TextureFormat::RGB16F:
        case TextureFormat::RGBA16F:
            return GL_FLOAT;
        case TextureFormat::Depth24:
            return GL_UNSIGNED_INT;
        case TextureFormat::Depth24Stencil8:
            return GL_UNSIGNED_INT_24_8;
        default:
            return GL_UNSIGNED_BYTE;
    }
}

GLuint Texture::CreateSamplerObject(TextureWrap wrapS, TextureWrap wrapT,
                                    TextureFilter minFilter, TextureFilter magFilter,
                                    float anisotropy) const {
    GLuint samplerID;
    glGenSamplers(1, &samplerID);
    glSamplerParameteri(samplerID, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapS));
    glSamplerParameteri(samplerID, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapT));
    glSamplerParameteri(samplerID, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glSamplerParameteri(samplerID, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    
    if (anisotropy > 0.0f) {
        GLfloat maxAnisotropy = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
        float clamped = std::min(anisotropy, maxAnisotropy);
        glSamplerParameterf(samplerID, GL_TEXTURE_MAX_ANISOTROPY, clamped);
    }
    
    return samplerID;
}

} // namespace Rendering
} // namespace OpenEye

