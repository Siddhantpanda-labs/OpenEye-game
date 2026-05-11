// =====================================================================================
// Textures.cpp — PIMPL impls for the public texture handles
// (Texture2D, TextureCube, Texture3D, TextureArray).
//
// Consolidated into one TU because the four handles share an identical PIMPL pattern
// (shared_ptr<internal>, IsValid, dimensions, native handle). One TU keeps copy/move
// boilerplate maintainable; one .obj per type would not buy us anything since the
// internal headers (`eyelib/rendering/resources/Texture*.h`) are all already included
// here transitively via `RHI.h`.
//
// Also defines `material_internal::ExtractInternalTexture2D` used by Material.cpp to
// thread a public Texture2D's internal pointer into material setters without exposing
// a second access seam in the public header.
// =====================================================================================

#include "api/rendering/Texture2D.h"
#include "api/rendering/Texture3D.h"
#include "api/rendering/TextureArray.h"
#include "api/rendering/TextureCube.h"

#include "api/rendering/internal/ResourceAccess.h"
#include "eyelib/rendering/resources/Texture2D.h"
#include "eyelib/rendering/resources/Texture3D.h"
#include "eyelib/rendering/resources/TextureArray.h"
#include "eyelib/rendering/resources/TextureCube.h"

#include <utility>

namespace eyelib {
namespace api {
namespace rendering {

// =====================================================================================
// Texture2D
// =====================================================================================

class Texture2D::Impl {
public:
    std::shared_ptr<eyelib::rendering::Texture2D> texture;
    /** Cache token assigned at `ResourceCacheAccess::Wrap` time. Stored on the public
     *  handle so material setters can write the token into `MaterialRecord::*Texture`
     *  slots without round-tripping through the cache. 0 = wrap-without-cache (sentinel). */
    uint32_t cacheToken = 0u;
};

Texture2D::Texture2D() noexcept : m_impl(std::make_unique<Impl>()) {}
Texture2D::~Texture2D() = default;

Texture2D::Texture2D(const Texture2D& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
Texture2D& Texture2D::operator=(const Texture2D& other) {
    if (this != &other) *m_impl = *other.m_impl;
    return *this;
}
Texture2D::Texture2D(Texture2D&& other) noexcept = default;
Texture2D& Texture2D::operator=(Texture2D&& other) noexcept = default;

bool Texture2D::IsValid() const noexcept {
    return m_impl && m_impl->texture && m_impl->texture->IsValid();
}

int Texture2D::GetWidth() const noexcept {
    return (m_impl && m_impl->texture) ? m_impl->texture->GetWidth() : 0;
}

int Texture2D::GetHeight() const noexcept {
    return (m_impl && m_impl->texture) ? m_impl->texture->GetHeight() : 0;
}

void* Texture2D::GetNativeHandle() const noexcept {
    return (m_impl && m_impl->texture) ? static_cast<void*>(m_impl->texture->GetTextureRHI()) : nullptr;
}

Texture2D Texture2DAccess::Wrap(std::shared_ptr<eyelib::rendering::Texture2D> internalTexture,
                                uint32_t cacheToken) {
    Texture2D handle{};
    handle.m_impl->texture    = std::move(internalTexture);
    handle.m_impl->cacheToken = cacheToken;
    return handle;
}

std::shared_ptr<eyelib::rendering::Texture2D> Texture2DAccess::Unwrap(const Texture2D& publicTexture) {
    return publicTexture.m_impl ? publicTexture.m_impl->texture : nullptr;
}

uint32_t Texture2DAccess::GetCacheToken(const Texture2D& publicTexture) noexcept {
    return publicTexture.m_impl ? publicTexture.m_impl->cacheToken : 0u;
}

// =====================================================================================
// TextureCube
// =====================================================================================

class TextureCube::Impl {
public:
    std::shared_ptr<eyelib::rendering::TextureCube> texture;
};

TextureCube::TextureCube() noexcept : m_impl(std::make_unique<Impl>()) {}
TextureCube::~TextureCube() = default;

TextureCube::TextureCube(const TextureCube& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
TextureCube& TextureCube::operator=(const TextureCube& other) {
    if (this != &other) *m_impl = *other.m_impl;
    return *this;
}
TextureCube::TextureCube(TextureCube&& other) noexcept = default;
TextureCube& TextureCube::operator=(TextureCube&& other) noexcept = default;

bool TextureCube::IsValid() const noexcept {
    return m_impl && m_impl->texture && m_impl->texture->IsValid();
}

int TextureCube::GetSize() const noexcept {
    return (m_impl && m_impl->texture) ? m_impl->texture->GetSize() : 0;
}

void* TextureCube::GetNativeHandle() const noexcept {
    return (m_impl && m_impl->texture) ? static_cast<void*>(m_impl->texture->GetTextureRHI()) : nullptr;
}

TextureCube TextureCubeAccess::Wrap(std::shared_ptr<eyelib::rendering::TextureCube> internalTexture) {
    TextureCube handle{};
    handle.m_impl->texture = std::move(internalTexture);
    return handle;
}

// =====================================================================================
// Texture3D
// =====================================================================================

class Texture3D::Impl {
public:
    std::shared_ptr<eyelib::rendering::Texture3D> texture;
};

Texture3D::Texture3D() noexcept : m_impl(std::make_unique<Impl>()) {}
Texture3D::~Texture3D() = default;

Texture3D::Texture3D(const Texture3D& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
Texture3D& Texture3D::operator=(const Texture3D& other) {
    if (this != &other) *m_impl = *other.m_impl;
    return *this;
}
Texture3D::Texture3D(Texture3D&& other) noexcept = default;
Texture3D& Texture3D::operator=(Texture3D&& other) noexcept = default;

bool Texture3D::IsValid() const noexcept {
    return m_impl && m_impl->texture && m_impl->texture->IsValid();
}

int Texture3D::GetWidth() const noexcept  { return (m_impl && m_impl->texture) ? m_impl->texture->GetWidth()  : 0; }
int Texture3D::GetHeight() const noexcept { return (m_impl && m_impl->texture) ? m_impl->texture->GetHeight() : 0; }
int Texture3D::GetDepth() const noexcept  { return (m_impl && m_impl->texture) ? m_impl->texture->GetDepth()  : 0; }

void* Texture3D::GetNativeHandle() const noexcept {
    if (!m_impl || !m_impl->texture) return nullptr;
    // Internal Texture3D does not currently expose GetTextureRHI(); the only stable
    // observation is `Id()` (backend-native id). Wrap as void* via uintptr_t.
    const auto id = static_cast<uintptr_t>(m_impl->texture->Id());
    return reinterpret_cast<void*>(id);
}

Texture3D Texture3DAccess::Wrap(std::shared_ptr<eyelib::rendering::Texture3D> internalTexture) {
    Texture3D handle{};
    handle.m_impl->texture = std::move(internalTexture);
    return handle;
}

// =====================================================================================
// TextureArray
// =====================================================================================

class TextureArray::Impl {
public:
    std::shared_ptr<eyelib::rendering::TextureArray> texture;
};

TextureArray::TextureArray() noexcept : m_impl(std::make_unique<Impl>()) {}
TextureArray::~TextureArray() = default;

TextureArray::TextureArray(const TextureArray& other) : m_impl(std::make_unique<Impl>(*other.m_impl)) {}
TextureArray& TextureArray::operator=(const TextureArray& other) {
    if (this != &other) *m_impl = *other.m_impl;
    return *this;
}
TextureArray::TextureArray(TextureArray&& other) noexcept = default;
TextureArray& TextureArray::operator=(TextureArray&& other) noexcept = default;

bool TextureArray::IsValid() const noexcept {
    return m_impl && m_impl->texture && m_impl->texture->IsValid();
}

int TextureArray::GetWidth() const noexcept      { return (m_impl && m_impl->texture) ? m_impl->texture->GetWidth()  : 0; }
int TextureArray::GetHeight() const noexcept     { return (m_impl && m_impl->texture) ? m_impl->texture->GetHeight() : 0; }
int TextureArray::GetLayerCount() const noexcept { return (m_impl && m_impl->texture) ? m_impl->texture->GetLayers() : 0; }

void* TextureArray::GetNativeHandle() const noexcept {
    if (!m_impl || !m_impl->texture) return nullptr;
    const auto id = static_cast<uintptr_t>(m_impl->texture->Id());
    return reinterpret_cast<void*>(id);
}

TextureArray TextureArrayAccess::Wrap(std::shared_ptr<eyelib::rendering::TextureArray> internalTexture) {
    TextureArray handle{};
    handle.m_impl->texture = std::move(internalTexture);
    return handle;
}

} // namespace rendering
} // namespace api
} // namespace eyelib
