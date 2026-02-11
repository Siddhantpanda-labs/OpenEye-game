#version 430 core

in vec2 v_UV;
in vec4 v_Color;

uniform sampler2D u_Texture;

layout(location = 0) out vec4 o_Color;

void main() {
    vec4 texColor = texture(u_Texture, v_UV);
    o_Color = texColor * v_Color;
}
