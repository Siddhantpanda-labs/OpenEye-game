#version 460 core

// Canonical mesh layout (all declared; pos/normal/uv used)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_PrevView;
uniform mat4 u_PrevProjection;

out vec3 v_WorldPos;
out vec3 v_WorldNormal;
out vec2 v_TexCoord;
out vec2 v_Velocity;

void main() {
    vec4 w = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = w.xyz;
    v_WorldNormal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_TexCoord = a_TexCoord;
    
    // Calculate current and previous screen positions
    vec4 currentPos = u_Projection * u_View * w;
    vec4 prevPos = u_PrevProjection * u_PrevView * w;
    
    // Convert to NDC
    vec2 currentNDC = currentPos.xy / currentPos.w;
    vec2 prevNDC = prevPos.xy / prevPos.w;
    
    // Calculate velocity (motion vector in NDC space [-1, 1])
    // Motion blur shader will multiply by texelSize to get proper screen-space offset
    v_Velocity = currentNDC - prevNDC;
    
    gl_Position = currentPos;
}
