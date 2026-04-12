#pragma once

#include "eyelib/rendering/resources/TextureTypes.h"
#include "eyelib/rendering/rhi/RHI.h"
#include <vector>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {

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
    bool Create(rhi::IDevice* device, int width, int height, int depth, TextureFormat format, const void* data = nullptr);
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
    bool IsValid() const { return m_textureRHI != nullptr; }
    unsigned int Id() const;

    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

private:
    rhi::IDevice* GetDevice() const;
    rhi::IDevice* m_injectedDevice = nullptr;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::ITexture* m_textureRHI = nullptr;
    int m_width = 0, m_height = 0, m_depth = 0;
    TextureFormat m_format = TextureFormat::RGBA8;
    std::vector<uint8_t> m_cpuData;
};

} // namespace rendering
} // namespace eyelib
