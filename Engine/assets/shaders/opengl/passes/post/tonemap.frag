#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_HDRTexture;
uniform float u_Exposure = 1.0;
uniform bool u_UseACES = true;

// ACES Filmic Tone Mapping (industry standard)
vec3 ACESFilmic(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Simple Reinhard tone mapping (fallback)
vec3 Reinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

void main() {
    vec3 hdrColor = texture(u_HDRTexture, v_TexCoord).rgb;
    
    // Apply exposure
    hdrColor *= u_Exposure;
    
    // Tone mapping
    vec3 ldrColor;
    if (u_UseACES) {
        ldrColor = ACESFilmic(hdrColor);
    } else {
        ldrColor = Reinhard(hdrColor);
    }
    
    // Gamma correction (output is sRGB)
    FragColor = vec4(ldrColor, 1.0);
}

