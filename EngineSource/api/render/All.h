#pragma once

#include "api/render/Renderer.h"
#include "api/render/TextureTypes.h"
#include "api/render/Texture.h"
#include "api/render/Texture3D.h"
#include "api/render/TextureCube.h"
#include "api/render/TextureArray.h"
#include "api/render/Mesh.h"
#include "api/render/Material.h"
#include "api/render/MaterialInstance.h"
#include "api/render/Shader.h"
#include "api/render/ShaderLoader.h"
#include "api/render/RenderPipeline.h"
#include "api/render/RenderScene.h"
#include "api/render/RenderTarget.h"
#include "api/render/LODManager.h"
#include "api/render/GBuffer.h"
#include "api/render/ShadowMap.h"
#include "api/render/IBL.h"
#include "api/render/FrameGraph.h"
#include "api/render/PostProcessor.h"
#include "api/render/Sprite.h"
#include "api/render/Text.h"
#include "api/render/Camera.h"
#include "api/render/Light.h"
#include "api/render/passes/GBufferPass.h"
#include "api/render/passes/ShadowPass.h"
#include "api/render/passes/LightingPass.h"
#include "api/render/passes/TransparencyPass.h"

namespace eyelib {
namespace api {
namespace render {
using ::eyelib::api::Renderer;
using ::eyelib::api::TextureFormat;
using ::eyelib::api::TextureFilter;
using ::eyelib::api::TextureWrap;
using ::eyelib::api::Texture;
using ::eyelib::api::Texture3D;
using ::eyelib::api::TextureCube;
using ::eyelib::api::TextureArray;
using ::eyelib::api::Mesh;
using ::eyelib::api::Material;
using ::eyelib::api::MaterialInstance;
using ::eyelib::api::Shader;
using ::eyelib::api::ShaderLoader;
using ::eyelib::api::RenderPipeline;
using ::eyelib::api::RenderScene;
using ::eyelib::api::Drawable;
using ::eyelib::api::CameraView;
using ::eyelib::api::RenderTarget;
using ::eyelib::api::LODSettings;
using ::eyelib::api::LODManager;
using ::eyelib::api::GBuffer;
using ::eyelib::api::ShadowMap;
using ::eyelib::api::IBL;
using ::eyelib::api::FrameGraph;
using ::eyelib::api::FGTextureDesc;
using ::eyelib::api::FGTextureHandle;
using ::eyelib::api::FGPassResources;
using ::eyelib::api::PostProcessor;
using ::eyelib::api::Sprite;
using ::eyelib::api::TextRenderer;
using ::eyelib::api::Font;
using ::eyelib::api::render::Camera;
using ::eyelib::api::render::DirectionalLight;
using ::eyelib::api::render::PointLight;
using ::eyelib::api::render::SpotLight;
using ::eyelib::api::GBufferPass;
using ::eyelib::api::ShadowPass;
using ::eyelib::api::LightingPass;
using ::eyelib::api::TransparencyPass;
} // namespace render
} // namespace api
} // namespace eyelib
