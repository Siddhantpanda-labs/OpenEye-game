#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec2 u_Resolution;
uniform float u_Intensity = 0.002; // Offset distance in screen space

void main() {
    vec2 texelSize = 1.0 / u_Resolution;
    
    // Calculate offset direction (radial from center)
    vec2 center = vec2(0.5, 0.5);
    vec2 dist = v_TexCoord - center;
    float distFromCenter = length(dist);
    vec2 offsetDir = normalize(dist);
    
    // Apply chromatic aberration: offset RGB channels differently
    float offset = u_Intensity * distFromCenter;
    
    // Red channel - offset outward
    vec2 redOffset = v_TexCoord + offsetDir * offset;
    float r = texture(u_Texture, redOffset).r;
    
    // Green channel - no offset (reference)
    float g = texture(u_Texture, v_TexCoord).g;
    
    // Blue channel - offset inward
    vec2 blueOffset = v_TexCoord - offsetDir * offset * 0.5;
    float b = texture(u_Texture, blueOffset).b;
    
    FragColor = vec4(r, g, b, 1.0);
}

