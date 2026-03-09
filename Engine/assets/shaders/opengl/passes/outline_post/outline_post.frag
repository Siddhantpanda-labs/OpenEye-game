#version 460 core

layout(binding = 0) uniform sampler2D u_SceneColor;
layout(binding = 1) uniform usampler2D u_EntityID;

uniform uint u_SelectedID;
uniform vec3 u_OutlineColor;
uniform vec2 u_InvSize;

in vec2 v_TexCoord;

out vec4 FragColor;

void main() {
    vec4 sceneColor = texture(u_SceneColor, v_TexCoord);
    uint centerID = texture(u_EntityID, v_TexCoord).r;
    if (centerID != u_SelectedID) {
        FragColor = sceneColor;
        return;
    }
    uint n0 = texture(u_EntityID, v_TexCoord + vec2(u_InvSize.x, 0.0)).r;
    uint n1 = texture(u_EntityID, v_TexCoord - vec2(u_InvSize.x, 0.0)).r;
    uint n2 = texture(u_EntityID, v_TexCoord + vec2(0.0, u_InvSize.y)).r;
    uint n3 = texture(u_EntityID, v_TexCoord - vec2(0.0, u_InvSize.y)).r;
    bool edge = (n0 != u_SelectedID) || (n1 != u_SelectedID) || (n2 != u_SelectedID) || (n3 != u_SelectedID);
    float mask = edge ? 1.0 : 0.0;
    FragColor = mix(sceneColor, vec4(u_OutlineColor, 1.0), mask);
}
