/*
    OpenGL 4.1 - Geometry Shader to enable layered rendering
*/

#version 410 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

flat in int vInstance[3];
flat out float layer;

void main()
{
    gl_Layer = vInstance[0];
    layer = float(gl_Layer) + 0.5;
    gl_Position = gl_in[0].gl_Position; EmitVertex();
    gl_Position = gl_in[1].gl_Position; EmitVertex();
    gl_Position = gl_in[2].gl_Position; EmitVertex();
    EndPrimitive();
}


