#version 450

layout(location = 0) out vec2 outColor;

void main() {
    float depth = gl_FragCoord.z;
    float dx = dFdx(depth);
    float dy = dFdy(depth);
    
    // Calculate moment2 with variance adjustment (polygon slope)
    float moment2 = depth * depth + 0.25 * (dx * dx + dy * dy);
    
    outColor = vec2(depth, moment2);
}
