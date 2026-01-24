#include "eyelib/render/IBL.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/FullscreenQuad.h"
#include "eyelib/core/Log.h"
#include "eyelib/math/Types.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace eyelib {
namespace render {

IBL::IBL() = default;

IBL::~IBL() {
    if (m_irradianceMap) { glDeleteTextures(1, &m_irradianceMap); m_irradianceMap = 0; }
    if (m_prefilterMap) { glDeleteTextures(1, &m_prefilterMap); m_prefilterMap = 0; }
    if (m_brdfLUT) { glDeleteTextures(1, &m_brdfLUT); m_brdfLUT = 0; }
    if (m_captureFBO) { glDeleteFramebuffers(1, &m_captureFBO); m_captureFBO = 0; }
    if (m_captureRBO) { glDeleteRenderbuffers(1, &m_captureRBO); m_captureRBO = 0; }
}

bool IBL::Initialize(unsigned int environmentCubemapID, uint32_t irradianceRes, uint32_t prefilterRes) {
    if (environmentCubemapID == 0) {
        core::Log::ErrorF("IBL::Initialize: Invalid environment cubemap ID");
        return false;
    }

    m_cubeMesh = CreateCubeMesh();
    if (!m_cubeMesh || !m_cubeMesh->IsValid()) {
        core::Log::ErrorF("IBL: Failed to create cube mesh");
        return false;
    }

    glGenFramebuffers(1, &m_captureFBO);
    glGenRenderbuffers(1, &m_captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, prefilterRes, prefilterRes);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_captureRBO);

    GenerateIrradianceMap(environmentCubemapID, irradianceRes);
    GeneratePrefilterMap(environmentCubemapID, prefilterRes);
    GenerateBRDFLUT(512);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!IsValid()) {
        core::Log::ErrorF("IBL::Initialize: Failed to generate IBL maps");
        return false;
    }

    core::Log::InfoF("IBL initialized successfully");
    return true;
}

std::shared_ptr<Mesh> IBL::CreateCubeMesh() {
    auto mesh = std::make_shared<Mesh>();
    std::vector<Vertex> vertices = {
        // Front face
        {{-1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        // Back face
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
        // Top face
        {{-1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        // Bottom face
        {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        // Right face
        {{ 1.0f, -1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
        // Left face
        {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}
    };
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Top
        12, 13, 14, 14, 15, 12, // Bottom
        16, 17, 18, 18, 19, 16, // Right
        20, 21, 22, 22, 23, 20  // Left
    };
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    mesh->UploadToGPU();
    return mesh;
}

void IBL::GenerateIrradianceMap(unsigned int environmentCubemapID, uint32_t resolution) {
    if (!m_cubeMesh || !m_cubeMesh->IsValid()) {
        core::Log::ErrorF("IBL: Cube mesh not available for irradiance generation");
        return;
    }

    auto irradianceShader = CreateIrradianceShader();
    if (!irradianceShader || !irradianceShader->IsValid()) {
        core::Log::ErrorF("IBL: Failed to create irradiance shader");
        return;
    }

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

    math::Mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    math::Mat4 captureViews[] = {
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3(-1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  1.0f,  0.0f), math::Vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f, -1.0f,  0.0f), math::Vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f,  1.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f, -1.0f), math::Vec3(0.0f, -1.0f,  0.0f))
    };

    irradianceShader->Bind();
    irradianceShader->SetInt("u_EnvironmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubemapID);

    for (unsigned int i = 0; i < 6; ++i) {
        irradianceShader->SetMat4("u_View", captureViews[i]);
        irradianceShader->SetMat4("u_Projection", captureProjection);
        glViewport(0, 0, resolution, resolution);
        glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_cubeMesh->Bind();
        m_cubeMesh->Draw();
        m_cubeMesh->Unbind();
    }

    irradianceShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    core::Log::InfoF("IBL: Irradiance map generated at %ux%u", resolution, resolution);
}

void IBL::GeneratePrefilterMap(unsigned int environmentCubemapID, uint32_t resolution) {
    if (!m_cubeMesh || !m_cubeMesh->IsValid()) {
        core::Log::ErrorF("IBL: Cube mesh not available for prefilter generation");
        return;
    }

    auto prefilterShader = CreatePrefilterShader();
    if (!prefilterShader || !prefilterShader->IsValid()) {
        core::Log::ErrorF("IBL: Failed to create prefilter shader");
        return;
    }

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

    math::Mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    math::Mat4 captureViews[] = {
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3(-1.0f,  0.0f,  0.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  1.0f,  0.0f), math::Vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f, -1.0f,  0.0f), math::Vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f,  1.0f), math::Vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(math::Vec3(0.0f), math::Vec3( 0.0f,  0.0f, -1.0f), math::Vec3(0.0f, -1.0f,  0.0f))
    };

    prefilterShader->Bind();
    prefilterShader->SetInt("u_EnvironmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubemapID);

    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
        unsigned int mipWidth = static_cast<unsigned int>(resolution * std::pow(0.5, mip));
        unsigned int mipHeight = mipWidth;
        glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);
        float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
        prefilterShader->SetFloat("u_Roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i) {
            prefilterShader->SetMat4("u_View", captureViews[i]);
            prefilterShader->SetMat4("u_Projection", captureProjection);
            glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            m_cubeMesh->Bind();
            m_cubeMesh->Draw();
            m_cubeMesh->Unbind();
        }
    }

    prefilterShader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    core::Log::InfoF("IBL: Prefilter map generated at %ux%u with %u mip levels", resolution, resolution, maxMipLevels);
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
        core::Log::ErrorF("IBL: Failed to create BRDF shader");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdfLUT, 0);

    glViewport(0, 0, resolution, resolution);
    shader->Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    static FullscreenQuad quad;
    quad.Render();
    shader->Unbind();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

std::shared_ptr<Shader> IBL::CreateIrradianceShader() {
    auto shader = std::make_shared<Shader>();
    if (shader->LoadFromFile("assets/shaders/passes/ibl/irradiance_convolution.vert",
                             "assets/shaders/passes/ibl/irradiance_convolution.frag")) {
        return shader;
    }
    core::Log::ErrorF("IBL: Failed to load irradiance convolution shader");
    return nullptr;
}

std::shared_ptr<Shader> IBL::CreatePrefilterShader() {
    auto shader = std::make_shared<Shader>();
    if (shader->LoadFromFile("assets/shaders/passes/ibl/prefilter.vert",
                             "assets/shaders/passes/ibl/prefilter.frag")) {
        return shader;
    }
    core::Log::ErrorF("IBL: Failed to load prefilter shader");
    return nullptr;
}

std::shared_ptr<Shader> IBL::CreateBRDFShader() {
    auto shader = std::make_shared<Shader>();
    const char* brdfVert = R"(
#version 430 core
layout(location=0) in vec2 a_Position;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_Position * 0.5 + 0.5;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";
    const char* brdfFrag = R"(
#version 430 core
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
    if (shader->BuildFromSource(brdfVert, brdfFrag)) {
        return shader;
    }
    return nullptr;
}

void IBL::BindForLighting(unsigned int irradianceUnit, unsigned int prefilterUnit, unsigned int brdfLUTUnit) const {
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

} // namespace render
} // namespace eyelib
