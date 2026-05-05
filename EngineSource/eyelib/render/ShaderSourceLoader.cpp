#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <sstream>
#include <unordered_set>

namespace eyelib {
namespace render {

namespace {
bool AppendShaderSource(const std::string& path,
                        std::string& out,
                        std::unordered_set<std::string>& includeStack,
                        int depth,
                        std::vector<std::string>* outDependencies) {
    if (depth > 32) {
        core::Log::ErrorF("Shader include depth exceeded: %s", path.c_str());
        return false;
    }

    const std::string normalized = platform::filesystem::NormalizePath(path);
    if (includeStack.count(normalized) != 0) {
        core::Log::ErrorF("Shader include cycle detected: %s", normalized.c_str());
        return false;
    }
    if (outDependencies) outDependencies->push_back(normalized);

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
                if (!AppendShaderSource(includePath, out, includeStack, depth + 1, outDependencies)) {
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
} // namespace

bool LoadShaderSource(const std::string& path, std::string& out) {
    std::unordered_set<std::string> includeStack;
    out.clear();
    return AppendShaderSource(path, out, includeStack, 0, nullptr);
}

bool LoadShaderSourceWithDependencies(const std::string& path, std::string& out, std::vector<std::string>* outDependencies) {
    std::unordered_set<std::string> includeStack;
    out.clear();
    if (outDependencies) outDependencies->clear();
    return AppendShaderSource(path, out, includeStack, 0, outDependencies);
}

} // namespace render
} // namespace eyelib
