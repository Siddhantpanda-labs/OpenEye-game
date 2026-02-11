#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform sampler2D g_Velocity; // Motion vectors
uniform vec2 u_Resolution;
uniform float u_Intensity = 1.0;
uniform int u_Samples = 8;

void main() {
    vec2 velocity = texture(g_Velocity, v_TexCoord).rg;
    
    // Skip if no motion
    if (length(velocity) < 0.001) {
        FragColor = texture(u_Texture, v_TexCoord);
        return;
    }
    
    vec3 color = texture(u_Texture, v_TexCoord).rgb;
    vec2 texelSize = 1.0 / u_Resolution;
    
    // Sample along velocity direction
    vec2 offset = velocity * u_Intensity * texelSize;
    float weight = 1.0 / float(u_Samples + 1);
    color *= weight;
    
    for (int i = 1; i <= u_Samples; ++i) {
        vec2 sampleCoord = v_TexCoord + offset * float(i);
        color += texture(u_Texture, sampleCoord).rgb * weight;
    }
    
    FragColor = vec4(color, 1.0);
}

