#include "eyelib/render/resources/Shader.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace eyelib {
namespace render {

Shader::Shader() = default;

Shader::~Shader() {
    Destroy();
}

Shader::Shader(Shader&& other) noexcept {
    m_program = other.m_program;
    m_bonesUBO = other.m_bonesUBO;
    m_bonesUBOSize = other.m_bonesUBOSize;
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    other.m_program = 0;
    other.m_bonesUBO = 0;
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_program = other.m_program;
    m_bonesUBO = other.m_bonesUBO;
    m_bonesUBOSize = other.m_bonesUBOSize;
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    other.m_program = 0;
    other.m_bonesUBO = 0;
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
    return *this;
}

namespace {
bool AppendShaderSource(const std::string& path,
                        std::string& out,
                        std::unordered_set<std::string>& includeStack,
                        int depth) {
    if (depth > 32) {
        core::Log::ErrorF("Shader include depth exceeded: %s", path.c_str());
        return false;
    }

    const std::string normalized = platform::filesystem::NormalizePath(path);
    if (includeStack.count(normalized) != 0) {
        core::Log::ErrorF("Shader include cycle detected: %s", normalized.c_str());
        return false;
    }

    std::vector<uint8_t> data = platform::filesystem::ReadFile(normalized);
    if (data.empty()) {
        core::Log::ErrorF("Shader include not found: %s", normalized.c_str());
        return false;
    }

    includeStack.insert(normalized);
    std::string src(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(src);
    std::string line;
    const std::string baseDir = platform::filesystem::GetDirectory(normalized);

    while (std::getline(stream, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        if (trimmed.rfind("#include", 0) == 0) {
            const size_t firstQuote = trimmed.find_first_of("\"<");
            const size_t lastQuote = trimmed.find_last_of("\">");
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote + 1) {
                const std::string includeFile = trimmed.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                const std::string includePath = platform::filesystem::NormalizePath(baseDir + "/" + includeFile);
                if (!AppendShaderSource(includePath, out, includeStack, depth + 1)) {
                    includeStack.erase(normalized);
                    return false;
                }
                continue;
            }
        }

        out.append(line);
        out.push_back('\n');
    }

    includeStack.erase(normalized);
    return true;
}

bool LoadShaderSource(const std::string& path, std::string& out) {
    std::unordered_set<std::string> includeStack;
    out.clear();
    return AppendShaderSource(path, out, includeStack, 0);
}
} // namespace

GLuint Shader::CompileStage(unsigned int stageType, const std::string& src) {
    const char* csrc = src.c_str();
    GLuint stage = glCreateShader(stageType);
    glShaderSource(stage, 1, &csrc, nullptr);
    glCompileShader(stage);

    GLint ok = 0;
    glGetShaderiv(stage, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetShaderiv(stage, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        log.resize(static_cast<size_t>(logLen > 0 ? logLen : 1));
        glGetShaderInfoLog(stage, logLen, nullptr, log.data());
        core::Log::ErrorF("Shader compile failed: %s", log.c_str());
        glDeleteShader(stage);
        return 0;
    }
    return stage;
}

bool Shader::BuildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    Destroy();

    GLuint vs = CompileStage(GL_VERTEX_SHADER, vertexSrc);
    if (!vs) return false;
    GLuint fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!fs) { glDeleteShader(vs); return false; }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &logLen);
        std::string log;
        log.resize(static_cast<size_t>(logLen > 0 ? logLen : 1));
        glGetProgramInfoLog(m_program, logLen, nullptr, log.data());
        core::Log::ErrorF("Shader link failed: %s", log.c_str());
        Destroy();
        return false;
    }

    return true;
}

bool Shader::LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSrc;
    if (!LoadShaderSource(vertexPath, vertexSrc)) {
        core::Log::ErrorF("Failed to load vertex shader: %s", vertexPath.c_str());
        return false;
    }

    std::string fragmentSrc;
    if (!LoadShaderSource(fragmentPath, fragmentSrc)) {
        core::Log::ErrorF("Failed to load fragment shader: %s", fragmentPath.c_str());
        return false;
    }

    return BuildFromSource(vertexSrc, fragmentSrc);
}

bool Shader::BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc, const rhi::VertexLayout& layout) {
    if (!device) return false;
    if (m_deviceRHI) Destroy(m_deviceRHI);
    rhi::IShader* vs = device->CreateShader(rhi::ShaderStage::Vertex, vertexSrc.c_str(), vertexSrc.size());
    rhi::IShader* fs = device->CreateShader(rhi::ShaderStage::Fragment, fragmentSrc.c_str(), fragmentSrc.size());
    if (!vs || !fs) {
        if (vs) device->DestroyShader(vs);
        if (fs) device->DestroyShader(fs);
        return false;
    }
    rhi::PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.vertexLayout = layout;
    desc.topology = rhi::PrimitiveTopology::TriangleList;
    desc.depthStencil.depthTest = true;
    desc.depthStencil.depthWrite = true;
    rhi::IPipeline* pipe = device->CreateGraphicsPipeline(desc);
    device->DestroyShader(vs);
    device->DestroyShader(fs);
    if (!pipe) return false;
    m_pipelineRHI = pipe;
    m_deviceRHI = device;
    return true;
}

bool Shader::LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath, const rhi::VertexLayout& layout) {
    if (!device) return false;
    std::string vertexSrc;
    if (!LoadShaderSource(vertexPath, vertexSrc)) return false;
    std::string fragmentSrc;
    if (!LoadShaderSource(fragmentPath, fragmentSrc)) return false;
    return BuildFromSource(device, vertexSrc, fragmentSrc, layout);
}

void Shader::Destroy() {
    if (m_deviceRHI) Destroy(m_deviceRHI);
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    if (m_bonesUBO) {
        glDeleteBuffers(1, &m_bonesUBO);
        m_bonesUBO = 0;
        m_bonesUBOSize = 0;
    }
}

void Shader::Destroy(rhi::IDevice* device) {
    if (!device || device != m_deviceRHI) return;
    if (m_pipelineRHI) {
        device->DestroyPipeline(m_pipelineRHI);
        m_pipelineRHI = nullptr;
    }
    m_deviceRHI = nullptr;
}

void Shader::Bind() const {
    if (m_program) glUseProgram(m_program);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

void Shader::SetBool(const std::string& name, bool value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform1i(loc, value ? 1 : 0);
}

void Shader::SetInt(const std::string& name, int value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform1i(loc, value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform1f(loc, value);
}

void Shader::SetVec2(const std::string& name, const math::Vec2& value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform2fv(loc, 1, &value.x);
}

void Shader::SetVec3(const std::string& name, const math::Vec3& value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform3fv(loc, 1, &value.x);
}

void Shader::SetVec4(const std::string& name, const math::Vec4& value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniform4fv(loc, 1, &value.x);
}

void Shader::SetMat4(const std::string& name, const math::Mat4& value) const {
    if (!m_program) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, &value[0][0]);
}

void Shader::SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const {
    if (!m_program || !values || count == 0) return;
    GLint loc = glGetUniformLocation(m_program, name.c_str());
    if (loc >= 0) {
        glUniformMatrix4fv(loc, static_cast<GLsizei>(count), GL_FALSE, &values[0][0][0]);
        return;
    }

    if (name != "u_Bones" && name != "u_BoneMatrices") return;

    const GLuint blockIndex = glGetUniformBlockIndex(m_program, "Bones");
    if (blockIndex == GL_INVALID_INDEX) return;

    glUniformBlockBinding(m_program, blockIndex, 3);

    if (!m_bonesUBO) {
        glGenBuffers(1, &m_bonesUBO);
    }

    const size_t maxBones = 512;
    const size_t clampedCount = (count > maxBones) ? maxBones : count;
    const size_t byteSize = clampedCount * sizeof(math::Mat4);

    glBindBuffer(GL_UNIFORM_BUFFER, m_bonesUBO);
    if (m_bonesUBOSize < byteSize) {
        glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(byteSize), values, GL_DYNAMIC_DRAW);
        m_bonesUBOSize = byteSize;
    } else {
        glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(byteSize), values);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_bonesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

} // namespace render
} // namespace eyelib

