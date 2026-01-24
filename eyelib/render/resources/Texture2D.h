#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

/**
 * @brief 2D texture. Create, load from file/memory, bind, filter/wrap, mipmaps.
 * Uses platform FileSystem for LoadFromFile; OpenGL implementation in .cpp.
 */
class Texture2D {
public:
    Texture2D();
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    /** Create empty texture. */
    bool Create(int width, int height, TextureFormat format, const void* data = nullptr);

    /** Load from file (uses platform::filesystem::ReadFile + stb_image). */
    bool LoadFromFile(const std::string& path, bool isSRGB = false, bool generateMipmaps = true);

    /** Load from image bytes (PNG/JPEG etc. via stb_image). */
    bool LoadFromMemory(const uint8_t* data, size_t size, bool isSRGB = false, bool generateMipmaps = true);

    /** Update subregion. Use (0,0,0,0) for full update. */
    void SetData(const void* data, int x = 0, int y = 0, int width = 0, int height = 0);

    /** Optional readback; slow. */
    std::vector<uint8_t> GetData() const;

    void Bind(int slot = 0) const;
    void GenerateMipmaps();

    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    TextureFormat GetFormat() const { return m_format; }
    bool IsValid() const { return m_id != 0; }

    /** Raw GL texture id for internal use (e.g. framebuffer attach). */
    unsigned int Id() const { return m_id; }

private:
    unsigned int m_id = 0;
    int m_width = 0;
    int m_height = 0;
    TextureFormat m_format = TextureFormat::RGBA8;
};

} // namespace render
} // namespace eyelib
