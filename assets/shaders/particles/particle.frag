#version 450 core

in vec2 v_TexCoord;
in vec4 v_Color;
in float v_AgeNorm;

uniform sampler2D u_ParticleTexture;  // Optional texture / flipbook atlas
uniform bool u_UseTexture;

// Flipbook controls
uniform int u_FlipbookCols;           // 0 disables flipbook path
uniform int u_FlipbookRows;
uniform int u_FlipbookFrames;
uniform bool u_FlipbookBlend;

// Soft particles
uniform bool u_SoftParticles;
uniform float u_SoftParticleFade;     // world units
uniform sampler2D u_SceneDepth;       // depth from GBuffer
uniform mat4 u_InvProjection;
uniform vec2 u_Viewport;              // screen size in pixels

out vec4 FragColor;

// Compute tile UV for given frame index in a rows x cols atlas
// baseUV is [0,1] for the quad, we map it to the correct tile in the atlas
vec2 flipbookUV(vec2 baseUV, int frame, int cols, int rows) {
    if (cols <= 0 || rows <= 0) return baseUV;
    
    // Clamp frame to valid range
    int clampedFrame = clamp(frame, 0, cols * rows - 1);
    
    // Calculate which tile (col, row) this frame is in
    int col = clampedFrame % cols;
    int row = clampedFrame / cols;
    
    // Tile size in normalized UV space
    vec2 tileSize = vec2(1.0 / float(cols), 1.0 / float(rows));
    
    // Offset to the tile's top-left corner
    vec2 tileOffset = vec2(float(col), float(row)) * tileSize;
    
    // Map baseUV [0,1] to the specific tile's UV space [tileOffset, tileOffset + tileSize]
    return baseUV * tileSize + tileOffset;
}

void main() {
    vec4 sampled = vec4(1.0);
    float alpha = 1.0;

    if (u_UseTexture && u_FlipbookCols > 0 && u_FlipbookRows > 0 && u_FlipbookFrames > 1) {
        // Animate by lifetime: age 0..1 -> frames [0, F-1]
        float f = clamp(v_AgeNorm * float(u_FlipbookFrames - 1), 0.0, float(u_FlipbookFrames - 1));
        int frameA = int(floor(f));
        int frameB = min(frameA + 1, u_FlipbookFrames - 1);
        float t = u_FlipbookBlend ? fract(f) : 0.0;
        vec2 uvA = flipbookUV(v_TexCoord, frameA, u_FlipbookCols, u_FlipbookRows);
        vec2 uvB = flipbookUV(v_TexCoord, frameB, u_FlipbookCols, u_FlipbookRows);
        vec4 cA = texture(u_ParticleTexture, uvA);
        vec4 cB = texture(u_ParticleTexture, uvB);
        sampled = mix(cA, cB, t);
        alpha = sampled.a;
    } else if (u_UseTexture) {
        sampled = texture(u_ParticleTexture, v_TexCoord);
        alpha = sampled.a;
    } else {
        // Fallback circular falloff
        vec2 center = vec2(0.5, 0.5);
        float dist = distance(v_TexCoord, center);
        alpha = 1.0 - smoothstep(0.3, 0.5, dist);
        sampled = vec4(1.0);
    }

    // Tint and final alpha
    vec3 color = sampled.rgb * v_Color.rgb;
    alpha *= v_Color.a;

    // Soft particles depth fade
    if (u_SoftParticles) {
        vec2 uv = gl_FragCoord.xy / u_Viewport;
        float sceneDepth = texture(u_SceneDepth, uv).r;
        // Reconstruct view-space Z for scene and particle
        float ndcZScene = sceneDepth * 2.0 - 1.0;
        float ndcZPart = gl_FragCoord.z * 2.0 - 1.0;
        vec4 scenePos = u_InvProjection * vec4(uv * 2.0 - 1.0, ndcZScene, 1.0);
        vec4 partPos = u_InvProjection * vec4(uv * 2.0 - 1.0, ndcZPart, 1.0);
        scenePos /= scenePos.w;
        partPos /= partPos.w;
        float sceneViewZ = -scenePos.z;
        float partViewZ = -partPos.z;
        float delta = sceneViewZ - partViewZ; // >0 when particle is behind geometry
        if (delta > 0.0) {
            float fade = clamp(delta / max(u_SoftParticleFade, 1e-4), 0.0, 1.0);
            alpha *= fade;
        }
    }

    // Premultiplied alpha output
    FragColor = vec4(color * alpha, alpha);
    if (FragColor.a < 0.01) discard;
}

