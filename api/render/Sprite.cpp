#include "api/render/Sprite.h"
#include "api/render/Texture.h"
#include "eyelib/render/2d/Sprite.h"

namespace eyelib {
namespace api {

class Sprite::Impl {
public:
    ::eyelib::render::Sprite sprite;
};

Sprite::Sprite() : m_impl(std::make_unique<Impl>()) {}

Sprite::~Sprite() = default;

Sprite::Sprite(Sprite&& other) noexcept = default;

Sprite& Sprite::operator=(Sprite&& other) noexcept = default;

void Sprite::SetTexture(std::shared_ptr<Texture> tex) {
    if (!tex) return;
    auto texImpl = tex->GetInternal();
    m_impl->sprite.SetTexture(texImpl);
}

void Sprite::SetUV(const UVRect& uv) {
    ::eyelib::render::UVRect r{uv.minU, uv.minV, uv.maxU, uv.maxV};
    m_impl->sprite.SetUV(r);
}

void Sprite::SetUV(float minU, float minV, float maxU, float maxV) {
    m_impl->sprite.SetUV(minU, minV, maxU, maxV);
}

void Sprite::SetColor(const math::Vec4& c) {
    m_impl->sprite.SetColor(c);
}

void Sprite::SetPosition(const math::Vec2& pos) {
    m_impl->sprite.SetPosition(pos);
}

void Sprite::SetRotation(float radians) {
    m_impl->sprite.SetRotation(radians);
}

void Sprite::SetScale(const math::Vec2& s) {
    m_impl->sprite.SetScale(s);
}

void Sprite::SetScale(float s) {
    m_impl->sprite.SetScale(s);
}

void Sprite::SetFlip(bool horizontal, bool vertical) {
    m_impl->sprite.SetFlip(horizontal, vertical);
}

void Sprite::SetPivot(const math::Vec2& p) {
    m_impl->sprite.SetPivot(p);
}

void Sprite::SetSize(const math::Vec2& s) {
    m_impl->sprite.SetSize(s);
}

Sprite::UVRect Sprite::GetUV() const {
    auto uv = m_impl->sprite.GetUV();
    return {uv.minU, uv.minV, uv.maxU, uv.maxV};
}

math::Vec4 Sprite::GetColor() const {
    return m_impl->sprite.GetColor();
}

math::Vec2 Sprite::GetPosition() const {
    return m_impl->sprite.GetPosition();
}

float Sprite::GetRotation() const {
    return m_impl->sprite.GetRotation();
}

math::Vec2 Sprite::GetScale() const {
    return m_impl->sprite.GetScale();
}

} // namespace api
} // namespace eyelib
