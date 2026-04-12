#version 460

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main() {
    // Flip V so sampled RT rows match vkCmdCopyImage (same texel order as transfer copy to swapchain).
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
