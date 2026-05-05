#include "eyelib/world/SkyboxAssetId.h"

#include <cctype>

namespace eyelib {
namespace world {
namespace skybox {
namespace {

std::string_view TrimAsciiWhitespace(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

bool AsciiEqualsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb))
            return false;
    }
    return true;
}

} // namespace

bool IsProceduralSkyboxId(std::string_view id) {
    const std::string_view t = TrimAsciiWhitespace(id);
    if (t.empty())
        return true;
    return AsciiEqualsIgnoreCase(t, kProceduralId);
}

std::string NormalizeSkyboxAssetId(std::string_view input) {
    const std::string_view t = TrimAsciiWhitespace(input);
    if (t.empty() || AsciiEqualsIgnoreCase(t, kProceduralId))
        return std::string(kProceduralId);
    if (AsciiEqualsIgnoreCase(t, kEveningAlias))
        return std::string(kDefaultEveningCrossPath);
    return std::string(t);
}

} // namespace skybox
} // namespace world
} // namespace eyelib
