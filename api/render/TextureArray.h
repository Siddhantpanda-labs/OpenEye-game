#pragma once

#include "api/render/TextureTypes.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render { class TextureArray; }
namespace api {

class TextureArray {
public:
    TextureArray();
    ~TextureArray();

    TextureArray(const TextureArray&) = delete;
    TextureArray& operator=(const TextureArray&) = delete;
    TextureArray(TextureArray&& other) noexcept;
    TextureArray& operator=(TextureArray&& other) noexcept;

    bool Create(int width, int height, int layers, TextureFormat format, const void* data = nullptr);
    void SetData(const void* data, int layer, int x = 0, int y = 0, int width = 0, int height = 0);

    void GenerateMipmaps();
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t);

    int GetWidth() const;
    int GetHeight() const;
    int GetLayers() const;
    TextureFormat GetFormat() const;
    bool IsValid() const;

    std::shared_ptr<::eyelib::render::TextureArray> GetInternal() const;
    static std::shared_ptr<TextureArray> WrapInternal(const std::shared_ptr<::eyelib::render::TextureArray>& texture);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
