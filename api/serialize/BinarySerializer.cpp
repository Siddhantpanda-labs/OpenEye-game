#include "BinarySerializer.h"

#include "eyelib/serialize/BinarySerializer.h"

namespace eyelib {
namespace api {
namespace serialize {

std::vector<uint8_t> BinarySerializer::SerializeScene(const api::Scene& scene, BinaryCompression compression) {
    return eyelib::serialize::BinarySerializer::SerializeScene(
        *scene.GetInternal(),
        static_cast<eyelib::serialize::BinaryCompression>(compression)
    );
}

bool BinarySerializer::DeserializeScene(api::Scene& scene, const std::vector<uint8_t>& data) {
    return eyelib::serialize::BinarySerializer::DeserializeScene(*scene.GetInternal(), data);
}

bool BinarySerializer::SaveToFile(const api::Scene& scene, const std::string& path, BinaryCompression compression) {
    return eyelib::serialize::BinarySerializer::SaveToFile(
        *scene.GetInternal(),
        path,
        static_cast<eyelib::serialize::BinaryCompression>(compression)
    );
}

bool BinarySerializer::LoadFromFile(api::Scene& scene, const std::string& path) {
    return eyelib::serialize::BinarySerializer::LoadFromFile(*scene.GetInternal(), path);
}

} // namespace serialize
} // namespace api
} // namespace eyelib
