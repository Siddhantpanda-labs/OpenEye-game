#version 460 core

uniform vec3 u_OutlineColor = vec3(1.0, 0.5, 0.0);

out vec4 FragColor;

void main() {
    FragColor = vec4(u_OutlineColor, 1.0);
}
