#version 450 core

in vec2 v_TexCoord;
in vec4 v_Color;

out vec4 FragColor;

void main() {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(v_TexCoord, center);
    float alpha = 1.0 - smoothstep(0.35, 0.5, dist);
    vec4 color = vec4(v_Color.rgb, v_Color.a * alpha);
    if (color.a < 0.01) discard;
    FragColor = color;
}
