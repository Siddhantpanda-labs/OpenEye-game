#include "eyelib/render/2d/Sprite.h"
#include "eyelib/render/resources/Texture2D.h"
#include <glm/gtc/matrix_transform.hpp>

namespace eyelib {
namespace render {

math::Mat4 Sprite::GetTransform() const {
    math::Vec2 scale = m_scale;
    if (m_size.x > 0.0f && m_size.y > 0.0f && m_texture && m_texture->IsValid()) {
        int w = m_texture->GetWidth();
        int h = m_texture->GetHeight();
        if (w > 0 && h > 0) {
            float uvW = m_uv.maxU - m_uv.minU;
            float uvH = m_uv.maxV - m_uv.minV;
            if (uvW > 0.0f && uvH > 0.0f) {
                float texW = static_cast<float>(w) * uvW;
                float texH = static_cast<float>(h) * uvH;
                scale.x = m_size.x / texW;
                scale.y = m_size.y / texH;
            }
        }
    }
    if (m_flipH) scale.x = -scale.x;
    if (m_flipV) scale.y = -scale.y;
    math::Mat4 t(1.0f);
    t = glm::translate(t, math::Vec3(m_position, 0.0f));
    t = glm::rotate(t, m_rotation, math::Vec3(0.0f, 0.0f, 1.0f));
    t = glm::translate(t, math::Vec3(-m_pivot.x * scale.x, -m_pivot.y * scale.y, 0.0f));
    t = glm::scale(t, math::Vec3(scale.x, scale.y, 1.0f));
    return t;
}

} // namespace render
} // namespace eyelib
