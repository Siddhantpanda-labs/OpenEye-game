#pragma once

namespace eyelib {
namespace api {

enum class TextureFormat {
    R8, RG8, RGB8, RGBA8,
    RGB16F, RGBA16F,
    Depth24, Depth24Stencil8,
    None
};

enum class TextureFilter {
    Nearest, Linear,
    NearestMipmapNearest, LinearMipmapNearest,
    NearestMipmapLinear, LinearMipmapLinear
};

enum class TextureWrap {
    Repeat, MirroredRepeat, ClampToEdge, ClampToBorder
};

} // namespace api
} // namespace eyelib
