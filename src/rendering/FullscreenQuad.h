#pragma once

#include <glad/glad.h>

namespace OpenEye {
namespace Rendering {

// Utility class for rendering fullscreen quads (for lighting pass, post-processing)
class FullscreenQuad {
public:
    FullscreenQuad();
    ~FullscreenQuad();
    
    // Non-copyable
    FullscreenQuad(const FullscreenQuad&) = delete;
    FullscreenQuad& operator=(const FullscreenQuad&) = delete;
    
    // Render fullscreen quad
    void Render() const;
    
    bool IsValid() const { return m_VAO != 0; }

private:
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    
    void CreateQuad();
};

} // namespace Rendering
} // namespace OpenEye

