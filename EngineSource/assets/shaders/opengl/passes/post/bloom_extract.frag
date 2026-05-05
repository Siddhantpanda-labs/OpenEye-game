#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_HDRTexture;
uniform float u_Threshold = 1.0;

void main() {
    vec3 color = texture(u_HDRTexture, v_TexCoord).rgb;
    
    // Calculate luminance
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Extract bright areas (above threshold)
    vec3 bloom = max(color - vec3(u_Threshold), vec3(0.0));
    
    // Optional: smooth threshold falloff
    float smoothFactor = smoothstep(u_Threshold, u_Threshold * 1.5, luminance);
    bloom *= smoothFactor;
    
    FragColor = vec4(bloom, 1.0);
}

