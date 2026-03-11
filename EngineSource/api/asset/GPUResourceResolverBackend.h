#pragma once

/**
 * @file GPUResourceResolverBackend.h
 * @brief Types for IGPUResourceResolver implementation (RendererGPUResourceCache). Re-exported from implementation.
 */

#include "api/asset/IGPUResourceResolver.h"
#include "api/asset/AssetId.h"
#include "api/asset/AssetTypes.h"
#include "eyelib/asset/runtime/asset_manager.h"
#include "eyelib/render/MaterialDescriptor.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Texture2D.h"
#include "eyelib/render/resources/Material.h"
#include "eyelib/render/rhi/RHI.h"
