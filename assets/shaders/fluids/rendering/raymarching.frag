/*
    OpenGL 4.1 Core - Fluid Raymarching - Fragment Shader
*/

#version 410 core

out vec4 FragColor; // output color

uniform vec2 InverseScreenSize; // inverse of the screen size

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 grid_size; // Number of voxels in each dimension

uniform float nearPlane; // near plane of the camera
uniform vec3 eyePos; // camera position
uniform vec3 cameraFront; // camera front vector
uniform vec3 cameraUp; // camera up vector 
uniform vec3 cameraRight; // camera right vector

// Raydata textures
uniform sampler2D RayDataBack; 
uniform sampler2D RayDataFront;

uniform sampler3D DensityTexture; // fluid texture
uniform sampler3D ObstacleTexture;
uniform sampler2D BackgroundTexture; 

in vec3 ogPos; // cube local space position
in vec3 texPos; // texture space position

// Lighting parameters
in vec3 lightDir; // light direction in view space
in vec3 vViewPosition; // fragment-camera vector in view space

uniform float rugosity; // rugosity - 0 : smooth, 1: rough
uniform float F0; // fresnel reflectance at normal incidence
uniform float Kd; // weight of diffuse reflection

const float PI = 3.14159265359;

// Fluid type selector: 0 = Gas, 1 = Liquid
uniform int u_FluidType;

// Computes the intersection of a ray with the near plane of the camera
vec3 NearPlaneIntersection(vec3 eyePos, vec3 end)
{
    vec3 p0 = eyePos + cameraFront * nearPlane;
    vec3 dir = normalize(end - eyePos);
    float b = dot(dir, cameraFront);
    float a = dot(p0 - eyePos, cameraFront);
    float t = a / b;
    vec3 p = eyePos + dir * t;
    return p;
}

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 TextureVoxelAlign(vec3 pos)
{
    pos *= (grid_size - 1.0);
    pos += 0.5;
    pos /= grid_size;
    return pos;
}

float interpolate_tricubic_fast(sampler3D tex, vec3 coord)
{
    vec3 nrOfVoxels = grid_size;
    vec3 coord_grid = coord * nrOfVoxels;
    vec3 index = floor(coord_grid);
    vec3 fraction = coord_grid - index;
    vec3 one_frac = 1.0 - fraction;

    vec3 w0 = 1.0/6.0 * one_frac*one_frac*one_frac;
    vec3 w1 = 2.0/3.0 * fraction*fraction*(2.0-fraction);
    vec3 w2 = 2.0/3.0 * one_frac*one_frac*(2.0-one_frac);
    vec3 w3 = 1.0/6.0 * fraction*fraction*fraction;

    vec3 g0 = w0 + w1;
    vec3 g1 = w2 + w3;
    vec3 mult = 1.0 / nrOfVoxels;
    vec3 h0 = mult * ((w1 / g0) + index);
    vec3 h1 = mult * ((w3 / g1) + index);

    float tex000 = texture(tex, h0).r;
    float tex100 = texture(tex, vec3(h1.x, h0.y, h0.z)).r;
    tex000 = mix(tex100, tex000, g0.x);
    float tex010 = texture(tex, vec3(h0.x, h1.y, h0.z)).r;
    float tex110 = texture(tex, vec3(h1.x, h1.y, h0.z)).r;
    tex010 = mix(tex110, tex010, g0.x);
    tex000 = mix(tex010, tex000, g0.y);
    float tex001 = texture(tex, vec3(h0.x, h0.y, h1.z)).r;
    float tex101 = texture(tex, vec3(h1.x, h0.y, h1.z)).r;
    tex001 = mix(tex101, tex001, g0.x);
    float tex011 = texture(tex, vec3(h0.x, h1.y, h1.z)).r;
    float tex111 = texture(tex, h1).r;
    tex011 = mix(tex111, tex011, g0.x);
    tex001 = mix(tex011, tex001, g0.y);
    return mix(tex001, tex000, g0.z);
}

// Raymarching for gas 
vec4 RaymarchingGas(vec3 start, vec3 dir)
{
    vec4 finalColor = vec4(0,0,0,0);
    vec3 sampleInterval = 0.5 / grid_size;
    float t = dot(sampleInterval, abs(dir)) / (length(dir) * length(dir));
    t /= 2.0;
    float stepSize = t;
    float densityScale = 2.0; // Increased scale for better visibility
    float sampledDensity = 0;
    
    // Start with random offset for jittering
    float offset = 0.5 * stepSize * rand(gl_FragCoord.xy);
    
    for(float i = offset; i <= 1.0; i += stepSize)
    {
        vec3 p = start + dir * i;
        sampledDensity = texture(DensityTexture, p).x * densityScale;
        
        // Lower threshold to catch even small density values
        if (sampledDensity > 0.0001) {
            // Color varies with density: high density = whiter, low density = darker gray
            float densityFactor = clamp(sampledDensity * 1.5, 0.0, 1.0);
            vec3 smokeColor = mix(vec3(0.3, 0.3, 0.35), vec3(0.95, 0.95, 1.0), densityFactor);
            
            // Exponential falloff for more realistic smoke appearance
            // Increased multiplier for better visibility
            float alpha = 1.0 - exp(-sampledDensity * 5.0);
            alpha *= (1.0 - finalColor.w); // Multiply by remaining transparency
            
            finalColor.xyz += smokeColor * alpha;
            finalColor.w += alpha;
            
            if (finalColor.w > 0.95)
                break;
        }
    }
    
    // Lower threshold to show even faint smoke
    if (finalColor.w < 0.001)
        return vec4(0.0);
    
    // Normalize color based on accumulated alpha
    if (finalColor.w > 0.0) {
        finalColor.xyz /= finalColor.w;
    }
    
    return finalColor;
}

vec3 ComputeGradient(vec3 pos)
{
    vec3 InverseGridSize = 1.0 / grid_size;
    float center = texture(DensityTexture, pos).r;
    float left = texture(DensityTexture, pos + vec3(-1, 0, 0) * InverseGridSize).r;
    float right = texture(DensityTexture, pos + vec3(1, 0, 0) * InverseGridSize).r;
    float bottom = texture(DensityTexture, pos + vec3(0, -1, 0) * InverseGridSize).r;
    float top = texture(DensityTexture, pos + vec3(0, 1, 0) * InverseGridSize).r;
    float back = texture(DensityTexture, pos + vec3(0, 0, -1) * InverseGridSize).r;
    float front = texture(DensityTexture, pos + vec3(0, 0, 1) * InverseGridSize).r;
    float obsLeft = texture(ObstacleTexture, pos + vec3(-1, 0, 0) * InverseGridSize).r;
    float obsRight = texture(ObstacleTexture, pos + vec3(1, 0, 0) * InverseGridSize).r;
    float obsBottom = texture(ObstacleTexture, pos + vec3(0, -1, 0) * InverseGridSize).r;
    float obsTop = texture(ObstacleTexture, pos + vec3(0, 1, 0) * InverseGridSize).r;
    float obsBack = texture(ObstacleTexture, pos + vec3(0, 0, -1) * InverseGridSize).r;
    float obsFront = texture(ObstacleTexture, pos + vec3(0, 0, 1) * InverseGridSize).r;
    if (obsLeft > 0.0) left = center;
    if (obsRight > 0.0) right = center;
    if (obsBottom > 0.0) bottom = center;
    if (obsTop > 0.0) top = center;
    if (obsBack > 0.0) back = center;
    if (obsFront > 0.0) front = center;
    vec3 gradient = 0.5 * vec3(right - left, top - bottom, front - back);
    return gradient;
}

float G1(float angle, float alpha)
{
    float r = (alpha + 1.0);
    float k = (r*r) / 8.0;
    float num   = angle;
    float denom = angle * (1.0 - k) + k;
    return num / denom;
}

vec3 ComputeLighting(vec3 color, vec3 normal)
{
    vec3 L = normalize(lightDir);
    float NdotL = max(dot(normal, L), 0.0);
    vec3 lambert = (Kd * color) / PI;
    vec3 specular = vec3(0.0);
    if (NdotL > 0.0)
    {
        vec3 V = normalize(vViewPosition);
        vec3 H = normalize(L + V);
        float NdotH = max(dot(normal, H), 0.0);
        float NdotV = max(dot(normal, V), 0.0);
        float VdotH = max(dot(V, H), 0.0);
        float alpha_Squared = rugosity * rugosity;
        float NdotH_Squared = NdotH * NdotH;
        float G2 = G1(NdotV, rugosity) * G1(NdotL, rugosity);
        float D = alpha_Squared;
        float denom = NdotH_Squared * (alpha_Squared - 1.0) + 1.0;
        D = D / (PI * denom * denom);
        vec3 F = vec3(pow(1.0 - VdotH, 5.0));
        F *= (1.0 - F0);
        F += F0;
        if (NdotV * NdotL > 0.0)
            specular = (F * G2 * D) / (4.0 * NdotV * NdotL);
    }
    return (lambert + specular) * NdotL;
}

// Raymarching for the liquid
vec4 RaymarchingLiquid(vec3 start, vec3 dir)
{
    vec3 fluidColor = vec3(52.0,120.0, 198.0) / 255.0;
    vec4 finalColor = vec4(0,0,0,0);
    float alpha = 0.0;
    float stepSize = 0.1;
    vec3 sampleInterval = 0.5 / grid_size;
    float t = dot(sampleInterval, abs(dir)) / (length(dir) * length(dir));
    float i = t + t * rand(gl_FragCoord.xy);
    vec3 p = start + dir * i;
    float curr = texture(DensityTexture, p).x;
    float prev = curr; 
    i += t;
    bool surfaceFound = false;
    vec3 surface;
    for(; i <= 1.0 && length(p - start) <= length(dir); i += t)
    {
        p = start + dir * i;
        curr = interpolate_tricubic_fast(DensityTexture, p);
        if (curr < 0.0)
            alpha += stepSize;
        if (curr * prev < 0.0 && !surfaceFound)
        {
            surfaceFound = true;
            surface = p;
        }
        if (surfaceFound && alpha >= 0.8)
            break;
    }
    alpha = min(alpha, 0.8);
    if (alpha > 0.0)
    {
        float lightingFactor = 0.6;
        vec3 surfaceNormal;
        if (surfaceFound)
            surfaceNormal = ComputeGradient(surface);
        else
        {
            surfaceNormal = - ComputeGradient(p);
            lightingFactor += lightingFactor * 0.8;
        }
        surfaceNormal.z *= -1.0;
        surfaceNormal = (view * vec4(surfaceNormal, 0.0)).xyz;
        vec2 refractionPos = gl_FragCoord.xy - vec2(dot(surfaceNormal, cameraRight), dot(surfaceNormal, cameraUp));
        surfaceNormal = normalize(surfaceNormal);
        vec3 refractionColor = texture(BackgroundTexture, refractionPos * InverseScreenSize).xyz;
        fluidColor = fluidColor * lightingFactor + ComputeLighting(fluidColor, surfaceNormal) * (1 - lightingFactor);
        alpha = max(alpha, 0.1);
        vec3 color = fluidColor * alpha + refractionColor * (1.0 - alpha);
        finalColor = vec4(color, 1.0);
    }
    return finalColor;
}

void main()
{
    vec4 rd_back = texture(RayDataBack, gl_FragCoord.xy * InverseScreenSize);
    vec4 rd_front = texture(RayDataFront, gl_FragCoord.xy * InverseScreenSize);
    if (rd_front.x < 0.0)
        discard;
    vec4 raydata = rd_back - rd_front;
    vec3 start, dir;
    float fragDepth = gl_FragCoord.z;
    if (raydata.w < 0.0)
    {
        vec3 mOgPos = (model * vec4(ogPos, 1.0)).xyz;
        start = NearPlaneIntersection(eyePos, mOgPos);
        vec4 mvpNearPlane = projection * view * vec4(start, 1.0);
        mvpNearPlane /= mvpNearPlane.w;
        fragDepth = mvpNearPlane.z; 
        start = (vec4(start, 1.0) * transpose(inverse(model))).xyz;
        start = (start + 1.0) / 2.0;
        start.z = 1.0 - start.z;
        start = clamp(start, 0.0, 1.0);
        start = TextureVoxelAlign(start);
        dir = raydata.xyz - start;
    }
    else
    {
        if (!gl_FrontFacing)
            discard;
        start = TextureVoxelAlign(texPos);
        dir = raydata.xyz;
    }
    gl_FragDepth = fragDepth;
    if (length(dir) <= 0.0)
        discard;
    if (u_FluidType == 0) {
        FragColor = RaymarchingGas(start, dir);
    } else {
        FragColor = RaymarchingLiquid(start, dir);
    }
}


