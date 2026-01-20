#version 450 core

in vec2 v_TexCoord;
in vec4 v_Color;
in float v_AgeNorm;
in vec3 v_WorldPos;
in vec3 v_ViewDir;
in float v_ParticleSize;

uniform sampler2D u_ParticleTexture;
uniform bool u_UseTexture;

// Flipbook controls
uniform int u_FlipbookCols;
uniform int u_FlipbookRows;
uniform int u_FlipbookFrames;
uniform bool u_FlipbookBlend;

// Soft particles
uniform bool u_SoftParticles;
uniform float u_SoftParticleFade;
uniform sampler2D u_SceneDepth;
uniform mat4 u_InvProjection;
uniform vec2 u_Viewport;

// Volumetric raymarching
uniform vec3 u_CameraPos;
uniform int u_RaymarchSteps = 32;
uniform float u_DensityScale = 1.0;

out vec4 FragColor;

// Simple 3D noise function (for density)
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    float n = i.x + i.y * 57.0 + 113.0 * i.z;
    return mix(
        mix(mix(hash(vec3(n + 0.0)), hash(vec3(n + 1.0)), f.x),
            mix(hash(vec3(n + 57.0)), hash(vec3(n + 58.0)), f.x), f.y),
        mix(mix(hash(vec3(n + 113.0)), hash(vec3(n + 114.0)), f.x),
            mix(hash(vec3(n + 170.0)), hash(vec3(n + 171.0)), f.x), f.y), f.z);
}

// Sample density at a point in particle space
float sampleDensity(vec3 pos) {
    // Transform to particle-local space (centered at origin, size normalized)
    vec3 localPos = (pos - v_WorldPos) / v_ParticleSize;
    float dist = length(localPos);
    
    // Spherical falloff
    float sphereFalloff = 1.0 - smoothstep(0.0, 1.0, dist);
    if (sphereFalloff <= 0.0) return 0.0;
    
    // Add noise for volumetric appearance
    float noise = noise3D(localPos * 4.0 + v_AgeNorm * 2.0);
    noise = noise * 0.5 + 0.5; // Normalize to [0,1]
    
    // Combine falloff with noise
    return sphereFalloff * noise * u_DensityScale;
}

// Raymarch through the particle volume
vec4 raymarch(vec3 rayStart, vec3 rayDir, float maxDist) {
    float stepSize = maxDist / float(u_RaymarchSteps);
    vec3 pos = rayStart;
    
    vec4 color = vec4(0.0);
    float transmittance = 1.0;
    
    for (int i = 0; i < u_RaymarchSteps; i++) {
        float density = sampleDensity(pos);
        
        if (density > 0.001) {
            // Absorption and scattering
            float absorption = density * stepSize * 2.0;
            float scattering = density * stepSize * 0.5;
            
            // Accumulate color (simple single-scattering approximation)
            vec4 sampleColor = v_Color;
            sampleColor.rgb *= scattering;
            sampleColor.a = absorption;
            
            color += sampleColor * transmittance;
            transmittance *= exp(-absorption);
            
            if (transmittance < 0.01) break; // Early exit
        }
        
        pos += rayDir * stepSize;
    }
    
    return vec4(color.rgb, 1.0 - transmittance);
}

void main() {
    // Calculate ray start and direction for raymarching
    vec3 rayStart = u_CameraPos;
    vec3 rayDir = normalize(v_WorldPos - u_CameraPos);
    
    // Estimate intersection with particle sphere
    vec3 toParticle = v_WorldPos - rayStart;
    float distToCenter = length(toParticle);
    float distAlongRay = dot(toParticle, rayDir);
    
    // Check if ray intersects particle sphere
    float sphereRadius = v_ParticleSize;
    float closestApproach = distToCenter * distToCenter - distAlongRay * distAlongRay;
    if (closestApproach > sphereRadius * sphereRadius) {
        discard; // Ray doesn't intersect particle
    }
    
    // Calculate entry and exit points
    float halfChord = sqrt(sphereRadius * sphereRadius - closestApproach);
    float entryDist = distAlongRay - halfChord;
    float exitDist = distAlongRay + halfChord;
    
    if (entryDist < 0.0) entryDist = 0.0;
    if (exitDist < entryDist) discard;
    
    // Perform raymarching
    vec3 entryPoint = rayStart + rayDir * entryDist;
    float raymarchDist = exitDist - entryDist;
    
    vec4 volumetricColor = raymarch(entryPoint, rayDir, raymarchDist);
    
    // Apply texture/flipbook if available
    vec4 sampled = vec4(1.0);
    if (u_UseTexture && u_FlipbookCols > 0 && u_FlipbookRows > 0 && u_FlipbookFrames > 1) {
        float f = clamp(v_AgeNorm * float(u_FlipbookFrames - 1), 0.0, float(u_FlipbookFrames - 1));
        int frameA = int(floor(f));
        int frameB = min(frameA + 1, u_FlipbookFrames - 1);
        float t = u_FlipbookBlend ? fract(f) : 0.0;
        
        int colA = frameA % u_FlipbookCols;
        int rowA = frameA / u_FlipbookCols;
        int colB = frameB % u_FlipbookCols;
        int rowB = frameB / u_FlipbookCols;
        
        vec2 tileSize = vec2(1.0 / float(u_FlipbookCols), 1.0 / float(u_FlipbookRows));
        vec2 uvA = v_TexCoord * tileSize + vec2(float(colA), float(rowA)) * tileSize;
        vec2 uvB = v_TexCoord * tileSize + vec2(float(colB), float(rowB)) * tileSize;
        
        vec4 cA = texture(u_ParticleTexture, uvA);
        vec4 cB = texture(u_ParticleTexture, uvB);
        sampled = mix(cA, cB, t);
    } else if (u_UseTexture) {
        sampled = texture(u_ParticleTexture, v_TexCoord);
    }
    
    // Combine volumetric color with texture
    vec3 finalColor = volumetricColor.rgb * sampled.rgb * v_Color.rgb;
    float alpha = volumetricColor.a * sampled.a * v_Color.a;
    
    // Soft particles depth fade
    if (u_SoftParticles) {
        vec2 uv = gl_FragCoord.xy / u_Viewport;
        float sceneDepth = texture(u_SceneDepth, uv).r;
        float ndcZScene = sceneDepth * 2.0 - 1.0;
        float ndcZPart = gl_FragCoord.z * 2.0 - 1.0;
        vec4 scenePos = u_InvProjection * vec4(uv * 2.0 - 1.0, ndcZScene, 1.0);
        vec4 partPos = u_InvProjection * vec4(uv * 2.0 - 1.0, ndcZPart, 1.0);
        scenePos /= scenePos.w;
        partPos /= partPos.w;
        float sceneViewZ = -scenePos.z;
        float partViewZ = -partPos.z;
        float delta = sceneViewZ - partViewZ;
        if (delta > 0.0) {
            float fade = clamp(delta / max(u_SoftParticleFade, 1e-4), 0.0, 1.0);
            alpha *= fade;
        }
    }
    
    // Premultiplied alpha output
    FragColor = vec4(finalColor * alpha, alpha);
    if (FragColor.a < 0.01) discard;
}
