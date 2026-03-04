#pragma once

#include "eyelib/render/resources/TextureTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/assets/AssetId.h"
#include <cstddef>

#include "eyelib/render/TextureDescriptor.h"
#include <cstdint>
#include <string>
#include <vector>

namespace eyelib {
namespace render {

/** GPU texture instance (device-bound). CPU representation is TextureDescriptor (Phase 3 asset vs instance). */
class Texture2D {
public:
    Texture2D();
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    /** Create empty texture via default device (if set). */
    bool Create(int width, int height, TextureFormat format, const void* data = nullptr);
    /** RHI path: create texture via device. */
    bool Create(rhi::IDevice* device, int width, int height, TextureFormat format, const void* data = nullptr);

    /** Load from file (uses platform::filesystem::ReadFile + stb_image). */
    bool LoadFromFile(const std::string& path, bool isSRGB = false, bool generateMipmaps = true);
    /** CPU descriptor for last LoadFromFile (path + options). Empty path if never loaded from file. */
    TextureDescriptor GetDescriptor() const;
    /** Create GPU instance from CPU descriptor (loads from file). */
    bool LoadFromDescriptor(rhi::IDevice* device, const TextureDescriptor& desc);

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
    void Destroy(rhi::IDevice* device);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    TextureFormat GetFormat() const { return m_format; }
    bool IsValid() const { return m_textureRHI != nullptr; }

    /** Backend-native texture id for interop (e.g. framebuffer attach). */
    unsigned int Id() const;
    /** RHI path: return texture when created via Create(device, ...). */
    rhi::ITexture* GetTextureRHI() const { return m_textureRHI; }

    /** Wrap an existing RHI texture (e.g. from IRenderTarget). Non-owning; do not destroy. */
    void WrapRHI(rhi::ITexture* texture);

    /** Map render-layer TextureFormat to RHI format (for use by RenderTarget etc.). */
    static rhi::RHITextureFormat TextureFormatToRHI(TextureFormat f);

    /** Set device to use for Create() without device, LoadFromMemory, SetData, Bind. */
    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

    void SetAssetId(const assets::AssetId& id) { m_assetId = id; }
    const assets::AssetId& GetAssetId() const { return m_assetId; }
    bool HasAssetId() const { return m_assetId.IsValid(); }

private:
    rhi::IDevice* GetDevice() const;
    static rhi::RHITextureFormat ToRHIFormat(TextureFormat f);

    int m_width = 0;
    int m_height = 0;
    TextureFormat m_format = TextureFormat::RGBA8;
    rhi::IDevice* m_injectedDevice = nullptr;
    rhi::IDevice* m_deviceRHI = nullptr;
    rhi::ITexture* m_textureRHI = nullptr;
    bool m_wrappedRHI = false;  // true = WrapRHI, do not destroy
    std::vector<uint8_t> m_cpuData;
    /** CPU reload state (Phase 2.6). GetDescriptor() returns it; LoadFromFile sets it; Reload uses LoadFromDescriptor(device, m_descriptor). */
    TextureDescriptor m_descriptor;

    assets::AssetId m_assetId = assets::AssetId::Invalid();
};

} // namespace render
} // namespace eyelib
