#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/TextureDescriptor.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
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
        case TextureFormat::RGB8_SRGB: return 3;
        case TextureFormat::RGBA8_SRGB: return 4;
        case TextureFormat::RGB16F: return 6;
        case TextureFormat::RGBA16F: return 8;
        case TextureFormat::Depth24: return 4;
        case TextureFormat::Depth24Stencil8: return 4;
        case TextureFormat::None: return 4;
        default: return 4;
    }
}

} // namespace

Texture2D::Texture2D() = default;

Texture2D::~Texture2D() {
    if (m_deviceRHI && m_textureRHI && !m_wrappedRHI) Destroy(m_deviceRHI);
    m_textureRHI = nullptr;
    m_wrappedRHI = false;
}

rhi::RHITextureFormat Texture2D::ToRHIFormat(TextureFormat f) {
    return TextureFormatToRHI(f);
}

rhi::RHITextureFormat Texture2D::TextureFormatToRHI(TextureFormat f) {
    switch (f) {
        case TextureFormat::R8: return rhi::RHITextureFormat::R8;
        case TextureFormat::RG8: return rhi::RHITextureFormat::RG8;
        case TextureFormat::RGB8: return rhi::RHITextureFormat::RGB8;
        case TextureFormat::RGBA8: return rhi::RHITextureFormat::RGBA8;
        case TextureFormat::RGB8_SRGB: return rhi::RHITextureFormat::RGB8_SRGB;
        case TextureFormat::RGBA8_SRGB: return rhi::RHITextureFormat::RGBA8_SRGB;
        case TextureFormat::RGB16F: return rhi::RHITextureFormat::RGB16F;
        case TextureFormat::RGBA16F: return rhi::RHITextureFormat::RGBA16F;
        case TextureFormat::Depth24: return rhi::RHITextureFormat::Depth24;
        case TextureFormat::Depth24Stencil8: return rhi::RHITextureFormat::Depth24Stencil8;
        case TextureFormat::None: return rhi::RHITextureFormat::None;
        default: return rhi::RHITextureFormat::RGBA8;
    }
}

static TextureFormat RHIFormatToTextureFormat(rhi::RHITextureFormat f) {
    switch (f) {
        case rhi::RHITextureFormat::R8: return TextureFormat::R8;
        case rhi::RHITextureFormat::RG8: return TextureFormat::RG8;
        case rhi::RHITextureFormat::RGB8: return TextureFormat::RGB8;
        case rhi::RHITextureFormat::RGBA8: return TextureFormat::RGBA8;
        case rhi::RHITextureFormat::RGB8_SRGB: return TextureFormat::RGB8_SRGB;
        case rhi::RHITextureFormat::RGBA8_SRGB: return TextureFormat::RGBA8_SRGB;
        case rhi::RHITextureFormat::RGB16F: return TextureFormat::RGB16F;
        case rhi::RHITextureFormat::RGBA16F: return TextureFormat::RGBA16F;
        case rhi::RHITextureFormat::Depth24: return TextureFormat::Depth24;
        case rhi::RHITextureFormat::Depth24Stencil8: return TextureFormat::Depth24Stencil8;
        case rhi::RHITextureFormat::None: return TextureFormat::None;
        default: return TextureFormat::RGBA8;
    }
}

void Texture2D::WrapRHI(rhi::ITexture* texture) {
    if (!texture) return;
    m_textureRHI = texture;
    m_wrappedRHI = true;
    m_deviceRHI = nullptr;
    m_width = static_cast<int>(texture->GetWidth());
    m_height = static_cast<int>(texture->GetHeight());
    m_format = RHIFormatToTextureFormat(texture->GetFormat());
    m_cpuData.clear();
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : m_width(other.m_width), m_height(other.m_height), m_format(other.m_format),
      m_injectedDevice(other.m_injectedDevice), m_deviceRHI(other.m_deviceRHI),
      m_textureRHI(other.m_textureRHI), m_wrappedRHI(other.m_wrappedRHI),
      m_descriptor(std::move(other.m_descriptor)) {
    other.m_injectedDevice = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_textureRHI = nullptr;
    other.m_wrappedRHI = false;
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        if (m_deviceRHI && m_textureRHI && !m_wrappedRHI) Destroy(m_deviceRHI);
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_injectedDevice = other.m_injectedDevice;
        m_deviceRHI = other.m_deviceRHI;
        m_textureRHI = other.m_textureRHI;
        m_wrappedRHI = other.m_wrappedRHI;
        m_descriptor = std::move(other.m_descriptor);
        other.m_injectedDevice = nullptr;
        other.m_deviceRHI = nullptr;
        other.m_textureRHI = nullptr;
        other.m_wrappedRHI = false;
    }
    return *this;
}

rhi::IDevice* Texture2D::GetDevice() const {
    return m_injectedDevice ? m_injectedDevice : m_deviceRHI;
}

bool Texture2D::Create(int width, int height, TextureFormat format, const void* data) {
    rhi::IDevice* device = GetDevice();
    if (!device) return false;
    return Create(device, width, height, format, data);
}

bool Texture2D::Create(rhi::IDevice* device, int width, int height, TextureFormat format, const void* data) {
    if (!device || width <= 0 || height <= 0) return false;
    if (m_deviceRHI) Destroy(m_deviceRHI);
    const size_t bpp = BytesPerPixel(format);
    const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height) * bpp;
    m_cpuData.resize(total);
    if (data) {
        std::memcpy(m_cpuData.data(), data, total);
    } else if (total > 0) {
        std::memset(m_cpuData.data(), 0, total);
    }
    rhi::TextureDesc desc;
    desc.type = rhi::TextureType::Texture2D;
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.format = ToRHIFormat(format);
    desc.usage = rhi::TextureUsage::ShaderResource;
    m_textureRHI = device->CreateTexture(desc, data ? data : (m_cpuData.empty() ? nullptr : m_cpuData.data()));
    if (!m_textureRHI) return false;
    m_width = width;
    m_height = height;
    m_format = format;
    m_deviceRHI = device;
    return true;
}

void Texture2D::Destroy(rhi::IDevice* device) {
    if (!device || device != m_deviceRHI) return;
    if (m_textureRHI && !m_wrappedRHI) { device->DestroyTexture(m_textureRHI); }
    m_textureRHI = nullptr;
    m_deviceRHI = nullptr;
    m_wrappedRHI = false;
}

bool Texture2D::LoadFromFile(const std::string& path, bool isSRGB, bool generateMipmaps) {
    rhi::IDevice* device = GetDevice();
    return device && LoadFromFile(device, path, isSRGB, generateMipmaps);
}

bool Texture2D::LoadFromFile(rhi::IDevice* device, const std::string& path, bool isSRGB, bool generateMipmaps) {
    if (!device) return false;
    m_descriptor.filePath = path;
    m_descriptor.isSRGB = isSRGB;
    m_descriptor.generateMipmaps = generateMipmaps;
    std::vector<uint8_t> bytes = eyelib::platform::filesystem::ReadFile(path);
    if (bytes.empty()) {
        core::Log::ErrorF("Texture2D: failed to read file: %s", path.c_str());
        return false;
    }
    return LoadFromMemory(device, bytes.data(), bytes.size(), isSRGB, generateMipmaps);
}

TextureDescriptor Texture2D::GetDescriptor() const {
    return m_descriptor;
}

bool Texture2D::LoadFromDescriptor(rhi::IDevice* device, const TextureDescriptor& desc) {
    if (!device || desc.filePath.empty()) return false;
    return LoadFromFile(device, desc.filePath, desc.isSRGB, desc.generateMipmaps);
}

bool Texture2D::LoadFromMemory(const uint8_t* data, size_t size, bool isSRGB, bool generateMipmaps) {
    rhi::IDevice* device = GetDevice();
    return device && LoadFromMemory(device, data, size, isSRGB, generateMipmaps);
}

bool Texture2D::LoadFromMemory(rhi::IDevice* device, const uint8_t* data, size_t size, bool isSRGB, bool generateMipmaps) {
    if (!data || size == 0 || !device) return false;

    stbi_set_flip_vertically_on_load(0);
    int w = 0, h = 0, ch = 0;
    unsigned char* decoded = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &ch, 0);
    if (!decoded) {
        core::Log::ErrorF("Texture2D: stb_image failed: %s", stbi_failure_reason());
        return false;
    }

    TextureFormat fmt = TextureFormat::RGBA8;
    if (ch == 1) fmt = TextureFormat::R8;
    else if (ch == 2) fmt = TextureFormat::RG8;
    else if (ch == 3) fmt = isSRGB ? TextureFormat::RGB8_SRGB : TextureFormat::RGB8;
    else if (ch == 4) fmt = isSRGB ? TextureFormat::RGBA8_SRGB : TextureFormat::RGBA8;

    if (!Create(device, w, h, fmt, decoded)) {
        stbi_image_free(decoded);
        return false;
    }

    stbi_image_free(decoded);
    return true;
}

void Texture2D::SetData(const void* data, int x, int y, int width, int height) {
    if (!data || m_width <= 0 || m_height <= 0) return;
    rhi::IDevice* device = GetDevice();
    if (!device) return;
    if (width <= 0) width = m_width;
    if (height <= 0) height = m_height;
    const size_t bpp = BytesPerPixel(m_format);
    if (m_cpuData.empty()) {
        m_cpuData.resize(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * bpp);
    }
    for (int row = 0; row < height; ++row) {
        const size_t dstOffset = (static_cast<size_t>(y + row) * static_cast<size_t>(m_width) + static_cast<size_t>(x)) * bpp;
        const size_t srcOffset = static_cast<size_t>(row) * static_cast<size_t>(width) * bpp;
        std::memcpy(m_cpuData.data() + dstOffset, static_cast<const uint8_t*>(data) + srcOffset, static_cast<size_t>(width) * bpp);
    }
    if (m_deviceRHI) Destroy(m_deviceRHI);
    Create(device, m_width, m_height, m_format, m_cpuData.data());
}

std::vector<uint8_t> Texture2D::GetData() const {
    return m_cpuData;
}

unsigned int Texture2D::Id() const {
    if (!m_textureRHI) return 0;
    void* native = m_textureRHI->GetNativeTexture();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0u;
}

void Texture2D::Bind(int slot) const {
    if (!m_textureRHI) return;
    rhi::IDevice* device = GetDevice();
    if (!device) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->BindTexture(m_textureRHI, static_cast<uint32_t>(slot));
}

void Texture2D::GenerateMipmaps() {
    (void)this;
}

void Texture2D::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    (void)minFilter;
    (void)magFilter;
}

void Texture2D::SetWrap(TextureWrap s, TextureWrap t) {
    (void)s;
    (void)t;
}

} // namespace render
} // namespace eyelib
