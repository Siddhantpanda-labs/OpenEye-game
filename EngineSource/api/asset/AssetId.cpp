#include "api/asset/AssetId.h"
#include "asset/core/guid.h"

namespace eyelib {
namespace api {

std::string AssetId::ToString() const {
    asset::Guid g;
    g.high = high;
    g.low = low;
    return g.ToString();
}

AssetId AssetId::FromString(const std::string& s) {
    asset::Guid g = asset::Guid::FromString(s);
    return AssetId{ g.high, g.low };
}

} // namespace api
} // namespace eyelib

