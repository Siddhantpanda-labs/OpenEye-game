#pragma once

#include "eyelib/math/Types.h"
#include <memory>

namespace eyelib {
namespace render {

class Texture2D;

/** UV rectangle: (minU, minV, maxU, maxV) in texture space [0,1]. */
struct UVRect {
    float minU = 0.0f, minV = 0.0f, maxU = 1.0f, maxV = 1.0f;
};

/**
 * @brief 2D sprite representation.
 * Texture, UV rect, color, transform (position, rotation, scale), flip.
 */
class Sprite {
public:
    Sprite() = default;

    void SetTexture(std::shared_ptr<Texture2D> tex) { m_texture = std::move(tex); }
    std::shared_ptr<Texture2D> GetTexture() const { return m_texture; }

    void SetUV(const UVRect& uv) { m_uv = uv; }
    void SetUV(float minU, float minV, float maxU, float maxV) {
        m_uv = { minU, minV, maxU, maxV };
    }
    const UVRect& GetUV() const { return m_uv; }

    void SetColor(const math::Vec4& c) { m_color = c; }
    const math::Vec4& GetColor() const { return m_color; }

    void SetPosition(const math::Vec2& pos) { m_position = pos; }
    const math::Vec2& GetPosition() const { return m_position; }

    void SetRotation(float radians) { m_rotation = radians; }
    float GetRotation() const { return m_rotation; }

    void SetScale(const math::Vec2& s) { m_scale = s; }
    void SetScale(float s) { m_scale = math::Vec2(s, s); }
    const math::Vec2& GetScale() const { return m_scale; }

    void SetFlip(bool horizontal, bool vertical) {
        m_flipH = horizontal;
        m_flipV = vertical;
    }
    bool GetFlipH() const { return m_flipH; }
    bool GetFlipV() const { return m_flipV; }

    /** Pivot in [0,1] (0,0)=bottom-left, (1,1)=top-right. Default (0.5,0.5) = center. */
    void SetPivot(const math::Vec2& p) { m_pivot = p; }
    const math::Vec2& GetPivot() const { return m_pivot; }

    /** Size in world units. If both > 0, scale is ignored for size. */
    void SetSize(const math::Vec2& s) { m_size = s; }
    const math::Vec2& GetSize() const { return m_size; }

    /** Build model matrix for 2D: position, rotation around pivot, scale/size. */
    math::Mat4 GetTransform() const;

private:
    std::shared_ptr<Texture2D> m_texture;
    UVRect m_uv;
    math::Vec4 m_color{1.0f, 1.0f, 1.0f, 1.0f};
    math::Vec2 m_position{0.0f, 0.0f};
    float m_rotation = 0.0f;
    math::Vec2 m_scale{1.0f, 1.0f};
    math::Vec2 m_pivot{0.5f, 0.5f};
    math::Vec2 m_size{0.0f, 0.0f};  // 0 = use scale
    bool m_flipH = false;
    bool m_flipV = false;
};

} // namespace render
} // namespace eyelib
