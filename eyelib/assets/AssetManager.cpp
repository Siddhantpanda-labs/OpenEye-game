#include "eyelib/assets/AssetManager.h"

namespace eyelib {
namespace assets {

AssetManager& AssetManager::Get() {
    static AssetManager instance;
    return instance;
}

} // namespace assets
} // namespace eyelib
