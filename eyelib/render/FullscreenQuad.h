#pragma once

namespace eyelib {
namespace render {

/** Fullscreen quad (NDC) for lighting pass, post-processing, etc. */
class FullscreenQuad {
public:
    FullscreenQuad();
    ~FullscreenQuad();

    FullscreenQuad(const FullscreenQuad&) = delete;
    FullscreenQuad& operator=(const FullscreenQuad&) = delete;

    void Render() const;
    bool IsValid() const { return m_vao != 0; }

private:
    void CreateQuad();

    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
};

} // namespace render
} // namespace eyelib
