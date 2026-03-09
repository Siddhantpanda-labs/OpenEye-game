layout(std140, binding = 1) uniform LightUniforms {
    vec4 u_DirectionalDir_Intensity; // xyz dir, w intensity
    vec4 u_DirectionalColor_Exposure; // rgb color, a exposure
    vec4 u_Ambient; // rgb ambient, a unused
};

#define u_LightDir (u_DirectionalDir_Intensity.xyz)
#define u_LightIntensity (u_DirectionalDir_Intensity.w)
#define u_LightColor (u_DirectionalColor_Exposure.rgb)
#define u_Exposure (u_DirectionalColor_Exposure.a)
#define u_AmbientLight (u_Ambient.rgb)


