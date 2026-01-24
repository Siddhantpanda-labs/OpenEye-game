#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace eyelib {
namespace render {
class Texture2D;
}
namespace api {

/**
 * @brief Texture handle (opaque). Load from file, bind, query size.
 * No OpenGL types exposed.
 */
class Texture {
public:
    Texture();
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /** Load from file. Returns false on failure. */
    bool LoadFromFile(const std::string& path, bool isSRGB = false, bool generateMipmaps = true);

    /** Create empty texture. */
    bool Create(int width, int height, int format, const void* data = nullptr);

    /** Get dimensions. */
    int GetWidth() const;
    int GetHeight() const;

    /** Check if valid. */
    bool IsValid() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class Material;
    friend class Sprite;
    
    /** Get internal texture (for Material/other internal use). */
    std::shared_ptr<render::Texture2D> GetInternal() const;
};

} // namespace api
} // namespace eyelib
