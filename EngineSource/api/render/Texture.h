#pragma once

#include "api/render/TextureTypes.h"
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

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

    /** Load from image bytes (PNG/JPEG etc.). */
    bool LoadFromMemory(const uint8_t* data, size_t size, bool isSRGB = false, bool generateMipmaps = true);

    /** Create empty texture. */
    bool Create(int width, int height, int format, const void* data = nullptr);
    bool Create(int width, int height, TextureFormat format, const void* data = nullptr);

    /** Update subregion. */
    void SetData(const void* data, int x = 0, int y = 0, int width = 0, int height = 0);

    /** Optional readback; slow. */
    std::vector<uint8_t> GetData() const;

    void GenerateMipmaps();
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t);

    /** Get dimensions. */
    int GetWidth() const;
    int GetHeight() const;
    TextureFormat GetFormat() const;

    /** Check if valid. */
    bool IsValid() const;

    /** Get internal texture (for internal use). */
    std::shared_ptr<::eyelib::render::Texture2D> GetInternal() const;
    static std::shared_ptr<Texture> WrapInternal(const std::shared_ptr<::eyelib::render::Texture2D>& texture);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    friend class Material;
    friend class Sprite;
};

} // namespace api
} // namespace eyelib
