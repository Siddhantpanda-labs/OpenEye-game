#pragma once

#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace OpenEye {
namespace Rendering {

enum class TextureFormat {
    RGB8,
    RGBA8,
    RGB16F,
    RGBA16F,
    R8,
    RG8,
    Depth24,
    Depth24Stencil8
};

enum class TextureWrap {
    Repeat = GL_REPEAT,
    MirroredRepeat = GL_MIRRORED_REPEAT,
    ClampToEdge = GL_CLAMP_TO_EDGE,
    ClampToBorder = GL_CLAMP_TO_BORDER
};

enum class TextureFilter {
    Nearest = GL_NEAREST,
    Linear = GL_LINEAR,
    NearestMipmapNearest = GL_NEAREST_MIPMAP_NEAREST,
    LinearMipmapNearest = GL_LINEAR_MIPMAP_NEAREST,
    NearestMipmapLinear = GL_NEAREST_MIPMAP_LINEAR,
    LinearMipmapLinear = GL_LINEAR_MIPMAP_LINEAR
};

struct TextureSpec {
    int width = 0;
    int height = 0;
    TextureFormat format = TextureFormat::RGBA8;
    TextureWrap wrapS = TextureWrap::Repeat;
    TextureWrap wrapT = TextureWrap::Repeat;
    TextureFilter minFilter = TextureFilter::LinearMipmapLinear;
    TextureFilter magFilter = TextureFilter::Linear;
    bool generateMipmaps = true;
    bool isSRGB = false;
    float anisotropy = 0.0f; // 0 = disabled, > 0 = enabled
};

class Texture {
public:
    Texture();
    ~Texture();
    
    // Non-copyable
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    
    // Movable
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    
    // Load from file
    bool LoadFromFile(const std::string& path, bool isSRGB = false, bool generateMipmaps = true);
    
    // Create from data
    bool Create(const TextureSpec& spec, const void* data = nullptr);
    
    // Update texture data
    void UpdateData(const void* data, int x = 0, int y = 0, int width = 0, int height = 0);
    
    // Bind texture to unit
    void Bind(int unit = 0) const;
    void Unbind(int unit = 0) const;
    
    // Get properties
    GLuint GetID() const { return m_textureID; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    TextureFormat GetFormat() const { return m_format; }
    bool IsValid() const { return m_textureID != 0; }
    
    // Set sampler parameters
    void SetWrap(TextureWrap wrapS, TextureWrap wrapT);
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetAnisotropy(float anisotropy);

private:
    GLuint m_textureID = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
    TextureFormat m_format = TextureFormat::RGBA8;
    bool m_isSRGB = false;
    GLenum m_internalFormat = GL_RGBA8;
    GLenum m_dataFormat = GL_RGBA;
    GLenum m_dataType = GL_UNSIGNED_BYTE;
    
    GLenum GetGLInternalFormat(TextureFormat format, bool isSRGB) const;
    GLenum GetGLDataFormat(TextureFormat format) const;
    GLenum GetGLDataType(TextureFormat format) const;
    GLuint CreateSamplerObject(TextureWrap wrapS, TextureWrap wrapT, 
                               TextureFilter minFilter, TextureFilter magFilter, 
                               float anisotropy) const;
};

} // namespace Rendering
} // namespace OpenEye

