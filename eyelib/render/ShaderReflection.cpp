#include "eyelib/render/ShaderReflection.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace eyelib {
namespace render {

namespace {

ShaderUniformType ParseType(const std::string& typeStr) {
    if (typeStr == "int") return ShaderUniformType::Int;
    if (typeStr == "float") return ShaderUniformType::Float;
    if (typeStr == "vec2") return ShaderUniformType::Vec2;
    if (typeStr == "vec3") return ShaderUniformType::Vec3;
    if (typeStr == "vec4") return ShaderUniformType::Vec4;
    if (typeStr == "mat4") return ShaderUniformType::Mat4;
    if (typeStr == "sampler2D") return ShaderUniformType::Sampler2D;
    if (typeStr == "samplerCube") return ShaderUniformType::SamplerCube;
    if (typeStr == "sampler2DShadow") return ShaderUniformType::Sampler2DShadow;
    return ShaderUniformType::Unknown;
}

void ParseUniformLine(const std::string& line, std::vector<ShaderUniformInfo>& out) {
    std::string s = line;
    auto end = s.find("//");
    if (end != std::string::npos) s.resize(end);
    size_t i = 0;
    while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == ';')) ++i;
    if (i >= s.size()) return;
    if (s.compare(i, 7, "uniform") != 0) return;
    i += 7;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i >= s.size()) return;
    size_t typeStart = i;
    while (i < s.size() && !std::isspace(static_cast<unsigned char>(s[i])) && s[i] != ';' && s[i] != '[') ++i;
    std::string typeStr = s.substr(typeStart, i - typeStart);
    ShaderUniformType type = ParseType(typeStr);
    if (type == ShaderUniformType::Unknown && typeStr.find("mat4") != std::string::npos)
        type = ShaderUniformType::Mat4;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (i >= s.size() || s[i] == ';') return;
    size_t nameStart = i;
    while (i < s.size() && s[i] != '[' && s[i] != ';' && s[i] != '=' && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    std::string name = s.substr(nameStart, i - nameStart);
    if (name.empty()) return;
    int arrayCount = 0;
    if (i < s.size() && s[i] == '[') {
        ++i;
        size_t numStart = i;
        while (i < s.size() && s[i] != ']') ++i;
        if (i < s.size()) {
            std::string numStr = s.substr(numStart, i - numStart);
            arrayCount = static_cast<int>(std::strtol(numStr.c_str(), nullptr, 10));
            if (arrayCount <= 0) arrayCount = 1;
            if (type == ShaderUniformType::Mat4) type = ShaderUniformType::Mat4Array;
        }
    }
    ShaderUniformInfo info;
    info.name = name;
    info.type = type;
    info.arrayCount = arrayCount;
    auto it = std::find_if(out.begin(), out.end(), [&name](const ShaderUniformInfo& u) { return u.name == name; });
    if (it == out.end())
        out.push_back(info);
}

void ParseSource(const std::string& source, std::vector<ShaderUniformInfo>& out) {
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("uniform") != std::string::npos)
            ParseUniformLine(line, out);
    }
}

} // namespace

const ShaderUniformInfo* ShaderReflection::Find(const std::string& name) const {
    auto it = std::find_if(uniforms.begin(), uniforms.end(), [&name](const ShaderUniformInfo& u) { return u.name == name; });
    return it != uniforms.end() ? &*it : nullptr;
}

bool ShaderReflection::HasUniform(const std::string& name) const {
    return Find(name) != nullptr;
}

ShaderReflection ParseShaderReflectionFromSource(const std::string& source) {
    ShaderReflection r;
    ParseSource(source, r.uniforms);
    return r;
}

ShaderReflection ParseShaderReflection(const std::string& vertexSrc, const std::string& fragmentSrc) {
    ShaderReflection r;
    ParseSource(vertexSrc, r.uniforms);
    ParseSource(fragmentSrc, r.uniforms);
    return r;
}

} // namespace render
} // namespace eyelib
