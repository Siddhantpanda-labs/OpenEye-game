#pragma once

#include "api/render/TextureTypes.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace render { class TextureCube; }
namespace api {

class TextureCube {
public:
    TextureCube();
    ~TextureCube();

    TextureCube(const TextureCube&) = delete;
    TextureCube& operator=(const TextureCube&) = delete;
    TextureCube(TextureCube&& other) noexcept;
    TextureCube& operator=(TextureCube&& other) noexcept;

    bool LoadFromFiles(const std::string& px, const std::string& nx,
                       const std::string& py, const std::string& ny,
                       const std::string& pz, const std::string& nz,
                       bool isSRGB = false, bool generateMipmaps = true);
    bool LoadFromFiles(const std::vector<std::string>& facePaths,
                       bool isSRGB = false, bool generateMipmaps = true);

    void GenerateMipmaps();
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);
    void SetWrap(TextureWrap s, TextureWrap t, TextureWrap r);

    int GetSize() const;
    bool IsValid() const;

    std::shared_ptr<render::TextureCube> GetInternal() const;
    static std::shared_ptr<TextureCube> WrapInternal(const std::shared_ptr<render::TextureCube>& texture);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
