#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform float u_Intensity = 0.5; // 0.0 = no vignette, 1.0 = full dark edges
uniform float u_Radius = 0.75; // Where vignette starts (0.0 = center, 1.0 = edges)
uniform float u_Softness = 0.5; // Smoothness of vignette edge

void main() {
    vec3 color = texture(u_Texture, v_TexCoord).rgb;
    
    // Calculate distance from center
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(v_TexCoord, center);
    
    // Apply vignette (radial darkening)
    float vignette = smoothstep(u_Radius, u_Radius + u_Softness, dist);
    vignette = 1.0 - (vignette * u_Intensity);
    
    color *= vignette;
    
    FragColor = vec4(color, 1.0);
}

