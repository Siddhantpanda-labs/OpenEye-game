#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {

/**
 * @brief 3D / volume texture. Create, SetData, Bind, filter/wrap, mipmaps.
 */
class Texture3D {
public:
    Texture3D();
    ~Texture3D();

    Texture3D(const Texture3D&) = delete;
    Texture3D& operator=(const Texture3D&) = delete;
    Texture3D(Texture3D&& other) noexcept;
    Texture3D& operator=(Texture3D&& other) noexcept;

    bool Create(int width, int height, int depth, TextureFormat format, const void* data = nullptr);
    void SetData(const void* data, int x = 0, int y = 0, int z = 0, int width = 0, int height = 0, int depth = 0);
    std::vector<uint8_t> GetData() const;

    void Bind(int slot = 0) const;
    void GenerateMipmaps();
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t, TextureWrap r);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetDepth() const { return m_depth; }
    TextureFormat GetFormat() const { return m_format; }
    bool IsValid() const { return m_id != 0; }
    unsigned int Id() const { return m_id; }

private:
    unsigned int m_id = 0;
    int m_width = 0, m_height = 0, m_depth = 0;
    TextureFormat m_format = TextureFormat::RGBA8;
};

} // namespace render
} // namespace eyelib
