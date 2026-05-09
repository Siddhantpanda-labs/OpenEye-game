#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D u_SceneColor;
layout(set = 0, binding = 1) uniform usampler2D u_EntityID;

layout(push_constant, std430) uniform OutlinePostPush {
    layout(offset = 0) uint u_SelectedID;
    layout(offset = 16) vec3 u_OutlineColor;
    layout(offset = 32) vec2 u_InvSize;
} pc;

void main() {
    vec4 sceneColor = texture(u_SceneColor, v_TexCoord);
    uint centerID = texture(u_EntityID, v_TexCoord).r;
    if (centerID != pc.u_SelectedID) {
        FragColor = sceneColor;
        return;
    }
    uint n0 = texture(u_EntityID, v_TexCoord + vec2(pc.u_InvSize.x, 0.0)).r;
    uint n1 = texture(u_EntityID, v_TexCoord - vec2(pc.u_InvSize.x, 0.0)).r;
    uint n2 = texture(u_EntityID, v_TexCoord + vec2(0.0, pc.u_InvSize.y)).r;
    uint n3 = texture(u_EntityID, v_TexCoord - vec2(0.0, pc.u_InvSize.y)).r;
    bool edge = (n0 != pc.u_SelectedID) || (n1 != pc.u_SelectedID) || (n2 != pc.u_SelectedID) || (n3 != pc.u_SelectedID);
    float mask = edge ? 1.0 : 0.0;
    FragColor = mix(sceneColor, vec4(pc.u_OutlineColor, 1.0), mask);
}
