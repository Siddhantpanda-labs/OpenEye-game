#include "api/assets/AssetId.h"

#include "eyelib/assets/AssetId.h"

namespace eyelib {
namespace api {

std::string AssetId::ToString() const {
    return eyelib::assets::AssetId(high, low).ToString();
}

AssetId AssetId::FromString(const std::string& s) {
    auto id = eyelib::assets::AssetId::FromString(s);
    return AssetId{ id.High(), id.Low() };
}

} // namespace api
} // namespace eyelib

