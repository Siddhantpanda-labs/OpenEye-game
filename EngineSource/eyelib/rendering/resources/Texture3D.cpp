#include "eyelib/rendering/resources/Texture3D.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include "eyelib/core/Log.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace eyelib {
namespace rendering {

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

Texture3D::Texture3D() = default;

Texture3D::~Texture3D() {
    if (m_deviceRHI && m_textureRHI) {
        m_deviceRHI->DestroyTexture(m_textureRHI);
        m_textureRHI = nullptr;
    }
}

Texture3D::Texture3D(Texture3D&& other) noexcept
    : m_injectedDevice(other.m_injectedDevice), m_deviceRHI(other.m_deviceRHI), m_textureRHI(other.m_textureRHI),
      m_width(other.m_width), m_height(other.m_height), m_depth(other.m_depth), m_format(other.m_format),
      m_cpuData(std::move(other.m_cpuData)) {
    other.m_injectedDevice = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_textureRHI = nullptr;
}

Texture3D& Texture3D::operator=(Texture3D&& other) noexcept {
    if (this != &other) {
        if (m_deviceRHI && m_textureRHI) m_deviceRHI->DestroyTexture(m_textureRHI);
        m_injectedDevice = other.m_injectedDevice;
        m_deviceRHI = other.m_deviceRHI;
        m_textureRHI = other.m_textureRHI;
        m_width = other.m_width;
        m_height = other.m_height;
        m_depth = other.m_depth;
        m_format = other.m_format;
        m_cpuData = std::move(other.m_cpuData);
        other.m_injectedDevice = nullptr;
        other.m_deviceRHI = nullptr;
        other.m_textureRHI = nullptr;
    }
    return *this;
}

rhi::IDevice* Texture3D::GetDevice() const {
    return m_injectedDevice ? m_injectedDevice : m_deviceRHI;
}

bool Texture3D::Create(int width, int height, int depth, TextureFormat format, const void* data) {
    rhi::IDevice* device = GetDevice();
    if (!device) return false;
    return Create(device, width, height, depth, format, data);
}

bool Texture3D::Create(rhi::IDevice* device, int width, int height, int depth, TextureFormat format, const void* data) {
    if (!device || width <= 0 || height <= 0 || depth <= 0) return false;
    if (m_deviceRHI && m_textureRHI) m_deviceRHI->DestroyTexture(m_textureRHI);
    const size_t bpp = BytesPerPixel(format);
    const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * bpp;
    m_cpuData.resize(total);
    if (data) {
        std::memcpy(m_cpuData.data(), data, total);
    } else if (total > 0) {
        std::memset(m_cpuData.data(), 0, total);
    }
    rhi::TextureDesc desc;
    desc.type = rhi::TextureType::Texture3D;
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.depthOrArrayLayers = static_cast<uint32_t>(depth);
    desc.format = Texture2D::TextureFormatToRHI(format);
    desc.usage = rhi::TextureUsage::ShaderResource;
    m_textureRHI = device->CreateTexture(desc, data ? data : (m_cpuData.empty() ? nullptr : m_cpuData.data()));
    if (!m_textureRHI) return false;
    m_deviceRHI = device;
    m_width = width;
    m_height = height;
    m_depth = depth;
    m_format = format;
    return true;
}

void Texture3D::SetData(const void* data, int x, int y, int z, int width, int height, int depth) {
    if (!data || m_width <= 0 || m_height <= 0 || m_depth <= 0) return;
    rhi::IDevice* device = GetDevice();
    if (!device) return;
    if (width <= 0) width = m_width;
    if (height <= 0) height = m_height;
    if (depth <= 0) depth = m_depth;
    const size_t bpp = BytesPerPixel(m_format);
    if (m_cpuData.empty()) {
        m_cpuData.resize(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * static_cast<size_t>(m_depth) * bpp);
    }
    for (int dz = 0; dz < depth; ++dz) {
        for (int dy = 0; dy < height; ++dy) {
            const size_t dst = (static_cast<size_t>(z + dz) * static_cast<size_t>(m_width) * static_cast<size_t>(m_height) +
                               static_cast<size_t>(y + dy) * static_cast<size_t>(m_width) +
                               static_cast<size_t>(x)) * bpp;
            const size_t src = (static_cast<size_t>(dz) * static_cast<size_t>(height) * static_cast<size_t>(width) +
                               static_cast<size_t>(dy) * static_cast<size_t>(width)) * bpp;
            std::memcpy(m_cpuData.data() + dst, static_cast<const uint8_t*>(data) + src, static_cast<size_t>(width) * bpp);
        }
    }
    Create(device, m_width, m_height, m_depth, m_format, m_cpuData.data());
}

std::vector<uint8_t> Texture3D::GetData() const {
    return m_cpuData;
}

void Texture3D::Bind(int slot) const {
    if (!m_textureRHI) return;
    rhi::IDevice* device = GetDevice();
    if (!device) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->BindTexture(m_textureRHI, static_cast<uint32_t>(slot));
}

void Texture3D::GenerateMipmaps() {
    (void)this;
}

void Texture3D::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    (void)minFilter;
    (void)magFilter;
}

void Texture3D::SetWrap(TextureWrap s, TextureWrap t, TextureWrap r) {
    (void)s;
    (void)t;
    (void)r;
}

unsigned int Texture3D::Id() const {
    if (!m_textureRHI) return 0;
    void* native = m_textureRHI->GetNativeTexture();
    return native ? static_cast<unsigned int>(reinterpret_cast<uintptr_t>(native)) : 0u;
}

} // namespace rendering
} // namespace eyelib
