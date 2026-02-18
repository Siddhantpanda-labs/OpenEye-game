#version 460 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform bool u_Horizontal = true;
uniform vec2 u_Resolution;
uniform float u_Intensity = 1.0; // Blur intensity multiplier (optional)

void main() {
    vec2 texelSize = 1.0 / u_Resolution;
    vec3 result = vec3(0.0);
    
    // Gaussian blur weights (9-tap)
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    
    result += texture(u_Texture, v_TexCoord).rgb * weights[0];
    
    // Apply intensity multiplier to blur radius
    float blurRadius = u_Intensity;
    
    if (u_Horizontal) {
        for (int i = 1; i < 5; ++i) {
            float offset = float(i) * blurRadius;
            result += texture(u_Texture, v_TexCoord + vec2(texelSize.x * offset, 0.0)).rgb * weights[i];
            result += texture(u_Texture, v_TexCoord - vec2(texelSize.x * offset, 0.0)).rgb * weights[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            float offset = float(i) * blurRadius;
            result += texture(u_Texture, v_TexCoord + vec2(0.0, texelSize.y * offset)).rgb * weights[i];
            result += texture(u_Texture, v_TexCoord - vec2(0.0, texelSize.y * offset)).rgb * weights[i];
        }
    }
    
    FragColor = vec4(result, 1.0);
}

