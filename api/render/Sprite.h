#pragma once

#include "eyelib/math/Types.h"
#include <memory>

namespace eyelib {
namespace api {

class Texture;

/**
 * @brief 2D sprite API (opaque). Set texture, UV, color, transform.
 */
class Sprite {
public:
    struct UVRect {
        float minU = 0.0f, minV = 0.0f, maxU = 1.0f, maxV = 1.0f;
    };

    Sprite();
    ~Sprite();

    Sprite(const Sprite&) = delete;
    Sprite& operator=(const Sprite&) = delete;
    Sprite(Sprite&& other) noexcept;
    Sprite& operator=(Sprite&& other) noexcept;

    void SetTexture(std::shared_ptr<Texture> tex);
    void SetUV(const UVRect& uv);
    void SetUV(float minU, float minV, float maxU, float maxV);
    void SetColor(const math::Vec4& c);
    void SetPosition(const math::Vec2& pos);
    void SetRotation(float radians);
    void SetScale(const math::Vec2& s);
    void SetScale(float s);
    void SetFlip(bool horizontal, bool vertical);
    void SetPivot(const math::Vec2& p);
    void SetSize(const math::Vec2& s);

    UVRect GetUV() const;
    math::Vec4 GetColor() const;
    math::Vec2 GetPosition() const;
    float GetRotation() const;
    math::Vec2 GetScale() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
