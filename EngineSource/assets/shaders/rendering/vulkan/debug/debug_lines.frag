// OpenEye next-gen rendering — editor debug-line overlay fragment.
// Writes interpolated vertex colour; alpha-blended over the HDR colour target.
#version 450

layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = vColor;
}
