#include "FullscreenQuad.h"
#include <glad/glad.h>

namespace OpenEye {
namespace Rendering {

FullscreenQuad::FullscreenQuad() {
    CreateQuad();
}

FullscreenQuad::~FullscreenQuad() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
    }
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
    }
}

void FullscreenQuad::CreateQuad() {
    // Fullscreen quad vertices (NDC coordinates)
    // Position and UV coordinates
    float vertices[] = {
        // Position (x, y)    UV (u, v)
        -1.0f, -1.0f,        0.0f, 0.0f,  // Bottom-left
         1.0f, -1.0f,        1.0f, 0.0f,  // Bottom-right
         1.0f,  1.0f,        1.0f, 1.0f,  // Top-right
        -1.0f,  1.0f,        0.0f, 1.0f   // Top-left
    };
    
    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };
    
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    
    GLuint EBO;
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(m_VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // UV attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    
    glBindVertexArray(0);
    
    // EBO is stored in VAO, so we can delete it
    glDeleteBuffers(1, &EBO);
}

void FullscreenQuad::Render() const {
    if (m_VAO == 0) {
        return;
    }
    
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

} // namespace Rendering
} // namespace OpenEye

