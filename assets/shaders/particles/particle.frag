#version 450 core

in vec2 v_TexCoord;
in vec4 v_Color;

uniform sampler2D u_ParticleTexture;  // Optional texture (if using textured particles)

out vec4 FragColor;

void main() {
    // Simple particle rendering (can be enhanced with texture later)
    // For now, use circular falloff for smooth edges
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(v_TexCoord, center);
    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);  // Soft circular falloff
    
    // Apply color with alpha
    FragColor = vec4(v_Color.rgb, v_Color.a * alpha);
    
    // Discard if too transparent
    if (FragColor.a < 0.01) {
        discard;
    }
}

