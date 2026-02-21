#include "eyelib/render/resources/TextureCube.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <stb_image.h>
#include <cstdint>
#include <cstring>

namespace eyelib {
namespace render {

namespace {
static size_t BytesPerPixel(TextureFormat f) {
    switch (f) {
        case TextureFormat::R8: return 1;
        case TextureFormat::RG8: return 2;
        case TextureFormat::RGB8: return 3;
        case TextureFormat::RGBA8: return 4;
        case TextureFormat::RGB16F: return 6;
        case TextureFormat::RGBA16F: return 8;
        case TextureFormat::None: return 4;
        default: return 4;
    }
}
} // namespace

TextureCube::TextureCube() = default;

TextureCube::~TextureCube() {
    if (m_deviceRHI && m_textureRHI) {
        m_deviceRHI->DestroyTexture(m_textureRHI);
        m_textureRHI = nullptr;
    }
}

TextureCube::TextureCube(TextureCube&& other) noexcept
    : m_injectedDevice(other.m_injectedDevice), m_deviceRHI(other.m_deviceRHI), m_textureRHI(other.m_textureRHI),
      m_size(other.m_size), m_cpuData(std::move(other.m_cpuData)) {
    other.m_injectedDevice = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_textureRHI = nullptr;
    other.m_size = 0;
}

TextureCube& TextureCube::operator=(TextureCube&& other) noexcept {
    if (this != &other) {
        if (m_deviceRHI && m_textureRHI) m_deviceRHI->DestroyTexture(m_textureRHI);
        m_injectedDevice = other.m_injectedDevice;
        m_deviceRHI = other.m_deviceRHI;
        m_textureRHI = other.m_textureRHI;
        m_size = other.m_size;
        m_cpuData = std::move(other.m_cpuData);
        other.m_injectedDevice = nullptr;
        other.m_deviceRHI = nullptr;
        other.m_textureRHI = nullptr;
        other.m_size = 0;
    }
    return *this;
}

bool TextureCube::LoadFromFiles(const std::vector<std::string>& facePaths, bool isSRGB, bool generateMipmaps) {
    if (facePaths.size() != 6) {
        core::Log::ErrorF("TextureCube: expected 6 face paths");
        return false;
    }
    std::vector<std::vector<uint8_t>> decodedFaces;
    decodedFaces.resize(6);
    int w = 0, h = 0, ch = 0;
    TextureFormat fmt = TextureFormat::RGBA8;
    for (size_t i = 0; i < facePaths.size(); ++i) {
        std::vector<uint8_t> bytes = platform::filesystem::ReadFile(facePaths[i]);
        if (bytes.empty()) {
            core::Log::ErrorF("TextureCube: failed to read file: %s", facePaths[i].c_str());
            return false;
        }
        int fw = 0, fh = 0, fch = 0;
        unsigned char* decoded = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &fw, &fh, &fch, 0);
        if (!decoded) {
            core::Log::ErrorF("TextureCube: stb_image failed: %s", stbi_failure_reason());
            return false;
        }
        if (i == 0) {
            w = fw; h = fh; ch = fch;
            if (ch == 1) fmt = TextureFormat::R8;
            else if (ch == 2) fmt = TextureFormat::RG8;
            else if (ch == 3) fmt = TextureFormat::RGB8;
            else fmt = TextureFormat::RGBA8;
        } else if (fw != w || fh != h || fch != ch) {
            stbi_image_free(decoded);
            core::Log::ErrorF("TextureCube: face size or format mismatch");
            return false;
        }
        size_t faceSize = static_cast<size_t>(fw) * static_cast<size_t>(fh) * static_cast<size_t>(ch);
        decodedFaces[i].assign(decoded, decoded + faceSize);
        stbi_image_free(decoded);
    }

    (void)generateMipmaps;
    rhi::IDevice* device = GetDevice();
    if (!device) return false;

    const size_t bpp = BytesPerPixel(fmt);
    const size_t faceBytes = static_cast<size_t>(w) * static_cast<size_t>(h) * bpp;
    m_cpuData.resize(faceBytes * 6);
    for (size_t i = 0; i < 6; ++i) {
        std::memcpy(m_cpuData.data() + i * faceBytes, decodedFaces[i].data(), faceBytes);
    }

    if (m_deviceRHI && m_textureRHI) m_deviceRHI->DestroyTexture(m_textureRHI);
    rhi::TextureDesc desc;
    desc.type = rhi::TextureType::TextureCube;
    desc.width = static_cast<uint32_t>(w);
    desc.height = static_cast<uint32_t>(h);
    desc.depthOrArrayLayers = 6;
    if (isSRGB && (fmt == TextureFormat::RGB8 || fmt == TextureFormat::RGBA8))
        desc.format = (fmt == TextureFormat::RGB8) ? rhi::RHITextureFormat::RGB8_SRGB : rhi::RHITextureFormat::RGBA8_SRGB;
    else
        desc.format = Texture2D::TextureFormatToRHI(fmt);
    desc.usage = rhi::TextureUsage::ShaderResource;
    m_textureRHI = device->CreateTexture(desc, m_cpuData.data());
    if (!m_textureRHI) return false;
    m_deviceRHI = device;
    m_size = w;
    return true;
}

bool TextureCube::LoadFromFiles(const std::string& px, const std::string& nx,
                               const std::string& py, const std::string& ny,
                               const std::string& pz, const std::string& nz,
                               bool isSRGB, bool generateMipmaps) {
    std::vector<std::string> faces = {px, nx, py, ny, pz, nz};
    return LoadFromFiles(faces, isSRGB, generateMipmaps);
}

rhi::IDevice* TextureCube::GetDevice() const {
    return m_injectedDevice ? m_injectedDevice : (m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice());
}

void TextureCube::Bind(int slot) const {
    if (!m_textureRHI) return;
    rhi::IDevice* device = GetDevice();
    if (!device) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->BindTexture(m_textureRHI, static_cast<uint32_t>(slot));
}

void TextureCube::GenerateMipmaps() {
    (void)this;
}

void TextureCube::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    (void)minFilter;
    (void)magFilter;
}

void TextureCube::SetWrap(TextureWrap s, TextureWrap t, TextureWrap r) {
    (void)s;
    (void)t;
    (void)r;
}

unsigned int TextureCube::Id() const {
    if (!m_textureRHI) return 0;
    void* native = m_textureRHI->GetNativeTexture();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0u;
}

} // namespace render
} // namespace eyelib
