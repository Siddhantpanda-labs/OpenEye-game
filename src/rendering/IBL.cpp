#include "IBL.h"
#include "resources/Cubemap.h"
#include "resources/Shader.h"
#include "FullscreenQuad.h"
#include "core/log/Log.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace OpenEye {
namespace Rendering {

IBL::IBL() = default;

IBL::~IBL() {
    if (m_irradianceMap) glDeleteTextures(1, &m_irradianceMap);
    if (m_prefilterMap) glDeleteTextures(1, &m_prefilterMap);
    if (m_brdfLUT) glDeleteTextures(1, &m_brdfLUT);
    if (m_captureFBO) glDeleteFramebuffers(1, &m_captureFBO);
    if (m_captureRBO) glDeleteRenderbuffers(1, &m_captureRBO);
}

bool IBL::Initialize(GLuint environmentCubemapID, uint32_t irradianceRes, uint32_t prefilterRes) {
    if (environmentCubemapID == 0) {
        OE_ERROR("IBL::Initialize: Invalid environment cubemap ID");
        return false;
    }

    // Create capture FBO
    glGenFramebuffers(1, &m_captureFBO);
    glGenRenderbuffers(1, &m_captureRBO);
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureRBO);

    // Generate IBL maps
    GenerateIrradianceMap(environmentCubemapID, irradianceRes);
    GeneratePrefilterMap(environmentCubemapID, prefilterRes);
    GenerateBRDFLUT(512);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!IsValid()) {
        OE_ERROR("IBL::Initialize: Failed to generate IBL maps");
        return false;
    }

    OE_INFO("IBL initialized successfully");
    return true;
}

void IBL::GenerateIrradianceMap(GLuint environmentCubemapID, uint32_t resolution) {
    // Create cubemap for irradiance (placeholder - full implementation requires cube mesh renderer)
    glGenTextures(1, &m_irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // TODO: Full implementation requires:
    // - Cube mesh renderer
    // - Irradiance convolution shader
    // - Render cube 6 times from each face
    OE_INFO("IBL: Irradiance map created (placeholder - generation will be implemented)");
}

void IBL::GeneratePrefilterMap(GLuint environmentCubemapID, uint32_t resolution) {
    glGenTextures(1, &m_prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    
    // TODO: Full implementation requires prefilter shader and rendering with varying roughness
    OE_INFO("IBL: Prefilter map created (placeholder - generation will be implemented)");
}

void IBL::GenerateBRDFLUT(uint32_t resolution) {
    glGenTextures(1, &m_brdfLUT);
    glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, resolution, resolution, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    auto shader = CreateBRDFShader();
    if (!shader || !shader->IsValid()) {
        OE_WARN("IBL: Failed to create BRDF shader, using placeholder");
        return;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);
    
    glViewport(0, 0, resolution, resolution);
    shader->Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Render fullscreen quad
    static FullscreenQuad quad;
    quad.Render();
    
    shader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void IBL::BindForLighting(GLuint irradianceUnit, GLuint prefilterUnit, GLuint brdfLUTUnit) const {
    if (m_irradianceMap) {
        glActiveTexture(GL_TEXTURE0 + irradianceUnit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradianceMap);
    }
    if (m_prefilterMap) {
        glActiveTexture(GL_TEXTURE0 + prefilterUnit);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilterMap);
    }
    if (m_brdfLUT) {
        glActiveTexture(GL_TEXTURE0 + brdfLUTUnit);
        glBindTexture(GL_TEXTURE_2D, m_brdfLUT);
    }
}

std::shared_ptr<Shader> IBL::CreateEquirectangularToCubemapShader() {
    // Placeholder - would create shader for equirectangular HDR to cubemap conversion
    return nullptr;
}

std::shared_ptr<Shader> IBL::CreateIrradianceShader() {
    auto shader = std::make_shared<Shader>();
    // Simplified: For now return null, full implementation would load irradiance shader
    // This requires a cube mesh renderer and proper shader files
    return shader;
}

std::shared_ptr<Shader> IBL::CreatePrefilterShader() {
    auto shader = std::make_shared<Shader>();
    return shader;
}

std::shared_ptr<Shader> IBL::CreateBRDFShader() {
    auto shader = std::make_shared<Shader>();
    // BRDF LUT generation shader
    const char* brdfVert = R"(
#version 450 core
layout(location=0) in vec2 a_Position;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_Position * 0.5 + 0.5;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";
    const char* brdfFrag = R"(
#version 450 core
out vec2 FragColor;
in vec2 v_TexCoord;
#define PI 3.14159265359
vec2 Hammersley(uint i, uint N);
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);

vec2 IntegrateBRDF(float NdotV, float roughness) {
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);
    float A = 0.0, B = 0.0;
    vec3 N = vec3(0.0, 0.0, 1.0);
    const uint SAMPLE_COUNT = 1024u;
    for(uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);
        if(NdotL > 0.0) {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.001);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return vec2(A, B);
}
vec2 Hammersley(uint i, uint N) {
    uint bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return vec2(float(i) / float(N), float(bits) * 2.3283064365386963e-10);
}
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return tangent * H.x + bitangent * H.y + N * H.z;
}
float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}
void main() {
    FragColor = IntegrateBRDF(v_TexCoord.x, v_TexCoord.y);
}
)";
    if (shader->Compile(brdfVert, brdfFrag)) {
        return shader;
    }
    return nullptr;
}

} // namespace Rendering
} // namespace OpenEye

