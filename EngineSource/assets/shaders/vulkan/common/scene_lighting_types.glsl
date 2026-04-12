#ifndef VK_SCENE_LIGHTING_TYPES_GLSL
#define VK_SCENE_LIGHTING_TYPES_GLSL

/** Matches CPU SceneLightHeader / FillSceneLightingBuffer (std140). */
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4

struct SceneLightHeader {
    vec3 direction; float pad0;
    vec3 color; float pad1;
    float intensity; float pad2; float pad3; float pad4;
    vec3 ambient; float pad5;
    float exposure; float pad6; float pad7; float pad8;
    vec3 viewPos; float pad9;
    mat4 lightSpaceMatrix;
    float shadowBias; float pad10; float pad11; float pad12;
    int useShadows; float pad13; float pad14; float pad15;
    int pointLightCount; int spotLightCount; float pad16; float pad17;
    int cascadeCount; float pad18; float pad19; float pad20;
    vec4 farBounds; // clip.z/clip.w at cascade far-plane centers (same as u_ViewProj metric)
};

struct PointLightData {
    vec3 position; float pad0;
    vec3 color; float intensity;
    float radius; float pad1; float pad2; float pad3;
};

struct SpotLightData {
    vec3 position; float pad0;
    vec3 direction; float pad1;
    vec3 color; float intensity;
    float range; float innerConeCos; float outerConeCos;
    float pad2; float pad3; float pad4; float pad5; float pad6;
};

#endif
