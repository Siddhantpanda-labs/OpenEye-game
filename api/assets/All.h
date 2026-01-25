#pragma once

#include "api/assets/AssetId.h"
#include "api/assets/AssetTypes.h"
#include "api/assets/AssetRegistry.h"
#include "api/assets/AssetLoader.h"
#include "api/assets/ModelAsset.h"
#include "api/assets/StreamingManager.h"
#include "api/assets/loaders/AnimationLoader.h"
#include "api/assets/loaders/AudioLoader.h"
#include "api/assets/loaders/MaterialLoader.h"
#include "api/assets/loaders/ModelLoader.h"
#include "api/assets/loaders/MeshLoader.h"
#include "api/assets/loaders/ShaderLoader.h"
#include "api/assets/loaders/TextureLoader.h"

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
using ::eyelib::api::ShaderLoader;
using ::eyelib::api::TextureLoader;
} // namespace assets
} // namespace api
} // namespace eyelib

