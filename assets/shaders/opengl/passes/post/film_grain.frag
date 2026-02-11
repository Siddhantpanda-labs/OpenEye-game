#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform float u_Intensity = 0.1;
uniform float u_Time = 0.0; // For animated grain

// Random function
float Random(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233)) + u_Time) * 43758.5453);
}

void main() {
    vec3 color = texture(u_Texture, v_TexCoord).rgb;
    
    // Generate grain noise
    float grain = Random(v_TexCoord * 1000.0) * 2.0 - 1.0; // -1 to 1
    grain *= u_Intensity;
    
    // Apply grain (additive noise)
    color += grain * 0.1;
    
    FragColor = vec4(color, 1.0);
}

