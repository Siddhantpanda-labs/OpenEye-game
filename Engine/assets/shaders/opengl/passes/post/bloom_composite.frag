#version 450 core

in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_SceneTexture;
uniform sampler2D u_BloomTexture;
uniform float u_BloomIntensity = 0.5;

void main() {
    vec3 scene = texture(u_SceneTexture, v_TexCoord).rgb;
    vec3 bloom = texture(u_BloomTexture, v_TexCoord).rgb;
    
    // Additive blending
    vec3 result = scene + bloom * u_BloomIntensity;
    
    FragColor = vec4(result, 1.0);
}

