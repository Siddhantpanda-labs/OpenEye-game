#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace render {

/**
 * @brief 2D array texture. Create(width, height, layers), Bind. For atlases, etc.
 */
class TextureArray {
public:
    TextureArray();
    ~TextureArray();

    TextureArray(const TextureArray&) = delete;
    TextureArray& operator=(const TextureArray&) = delete;
    TextureArray(TextureArray&& other) noexcept;
    TextureArray& operator=(TextureArray&& other) noexcept;

    bool Create(int width, int height, int layers, TextureFormat format, const void* data = nullptr);
    bool Create(rhi::IDevice* device, int width, int height, int layers, TextureFormat format, const void* data = nullptr);
    void SetData(const void* data, int layer, int x = 0, int y = 0, int width = 0, int height = 0);

    void Bind(int slot = 0) const;
    void GenerateMipmaps();
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetLayers() const { return m_layers; }
    TextureFormat GetFormat() const { return m_format; }
    bool IsValid() const { return m_textureRHI != nullptr; }
    unsigned int Id() const;

private:
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::ITexture* m_textureRHI = nullptr;
    int m_width = 0, m_height = 0, m_layers = 0;
    TextureFormat m_format = TextureFormat::RGBA8;
    std::vector<uint8_t> m_cpuData;
};

} // namespace render
} // namespace eyelib
