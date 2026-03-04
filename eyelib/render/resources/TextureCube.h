#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

/**
 * @brief Cubemap texture. LoadFromFiles(6 faces), Bind. For skyboxes, IBL.
 */
class TextureCube {
public:
    TextureCube();
    ~TextureCube();

    TextureCube(const TextureCube&) = delete;
    TextureCube& operator=(const TextureCube&) = delete;
    TextureCube(TextureCube&& other) noexcept;
    TextureCube& operator=(TextureCube&& other) noexcept;

    /** Load 6 faces: +X, -X, +Y, -Y, +Z, -Z. Uses platform FileSystem + stb_image. Device required for GPU upload. */
    bool LoadFromFiles(rhi::IDevice* device, const std::string& px, const std::string& nx,
                      const std::string& py, const std::string& ny,
                      const std::string& pz, const std::string& nz,
                      bool isSRGB = false, bool generateMipmaps = true);

    /** Overload: single array of 6 paths. */
    bool LoadFromFiles(rhi::IDevice* device, const std::vector<std::string>& facePaths, bool isSRGB = false, bool generateMipmaps = true);

    void Bind(int slot = 0) const;
    void GenerateMipmaps();
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t, TextureWrap r);

    int GetSize() const { return m_size; }
    bool IsValid() const { return m_textureRHI != nullptr; }
    unsigned int Id() const;
    rhi::ITexture* GetTextureRHI() const { return m_textureRHI; }

private:
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::ITexture* m_textureRHI = nullptr;
    int m_size = 0;
    std::vector<uint8_t> m_cpuData;
};

} // namespace render
} // namespace eyelib
