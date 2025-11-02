#pragma once

#include <string>
#include <vector>
#include <glad/glad.h>

namespace OpenEye {
namespace Rendering {

class Cubemap {
public:
    Cubemap() = default;
    ~Cubemap();

    // Non-copyable
    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;

    // Movable
    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    // Load from 6 file paths in order:
    // +X (right), -X (left), +Y (up), -Y (down), +Z (front), -Z (back)
    bool LoadFromFiles(const std::vector<std::string>& facePaths, bool srgb = false);
    
    // Create a solid color cubemap (for procedural environments)
    bool CreateSolidColor(float r, float g, float b, uint32_t resolution = 256);

    void Bind(GLuint unit = 0) const;
    void Unbind(GLuint unit = 0) const;

    GLuint GetID() const { return m_textureID; }
    bool IsValid() const { return m_textureID != 0; }

private:
    GLuint m_textureID = 0;
};

} // namespace Rendering
} // namespace OpenEye


