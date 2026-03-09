#version 460 core

uniform uint u_EntityID;

layout(location = 0) out uint outEntityID;

void main() {
    outEntityID = u_EntityID;
}
