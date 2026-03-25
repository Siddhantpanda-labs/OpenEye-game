#version 450

layout(location = 0) out uint outEntityID;

layout(push_constant, std430) uniform EntityIdFragPush {
    layout(offset = 192) uint u_EntityID;
} pc;

void main() {
    outEntityID = pc.u_EntityID;
}
