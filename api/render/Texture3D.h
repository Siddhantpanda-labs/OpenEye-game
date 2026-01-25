#pragma once

#include "api/render/TextureTypes.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
namespace render { class Texture3D; }
namespace api {

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

    void GenerateMipmaps();
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t, TextureWrap r);

    int GetWidth() const;
    int GetHeight() const;
    int GetDepth() const;
    TextureFormat GetFormat() const;
    bool IsValid() const;

    std::shared_ptr<render::Texture3D> GetInternal() const;
    static std::shared_ptr<Texture3D> WrapInternal(const std::shared_ptr<render::Texture3D>& texture);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
