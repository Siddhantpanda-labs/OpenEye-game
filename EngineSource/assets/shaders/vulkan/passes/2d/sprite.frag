#version 460

layout(location = 0) in vec2 v_UV;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_Texture;

void main() {
    o_Color = texture(u_Texture, v_UV) * v_Color;
}
