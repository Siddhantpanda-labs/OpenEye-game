#pragma once

#include "api/asset/AssetId.h"
#include "api/asset/AssetTypes.h"
#include "api/asset/AssetRegistry.h"
#include "api/asset/AssetLoader.h"
#include "api/asset/ModelAsset.h"
#include "api/asset/StreamingManager.h"
#include "api/asset/loaders/AnimationLoader.h"
#include "api/asset/loaders/AudioLoader.h"
#include "api/asset/loaders/MaterialLoader.h"
#include "api/asset/loaders/ModelLoader.h"
#include "api/asset/loaders/MeshLoader.h"
#include "api/asset/loaders/TextureLoader.h"

namespace eyelib {
namespace api {
namespace assets {
using ::eyelib::api::AssetId;
using ::eyelib::api::AssetType;
using ::eyelib::api::AssetRegistry;
using ::eyelib::api::AssetLoader;
using ::eyelib::api::ModelAsset;
using ::eyelib::api::StreamingManager;
using ::eyelib::api::AnimationLoader;
using ::eyelib::api::AudioLoader;
using ::eyelib::api::MaterialLoader;
using ::eyelib::api::ModelLoader;
using ::eyelib::api::MeshLoader;
using ::eyelib::api::TextureLoader;
} // namespace assets
} // namespace api
} // namespace eyelib

