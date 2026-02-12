#pragma once

#include "eyelib/render/IRenderSceneView.h"
#include "eyelib/render/RenderTypes.h"
#include <functional>

namespace eyelib {
namespace api {

struct RenderScene;

/** Read-only view over api::RenderScene implementing engine IRenderSceneView (zero full-scene copy). */
class ApiRenderSceneView : public ::eyelib::render::IRenderSceneView {
public:
    explicit ApiRenderSceneView(const RenderScene& scene) : m_scene(scene) {}
    ::eyelib::render::CameraView GetCamera() const override;
    ::eyelib::render::LightParams GetLight() const override;
    std::shared_ptr<::eyelib::render::TextureCube> GetSkybox() const override;
    void ForEachOpaqueDrawable(std::function<void(const ::eyelib::render::Drawable&)> cb) const override;
    void ForEachTransparentDrawable(std::function<void(const ::eyelib::render::Drawable&)> cb) const override;
    void ForEachPointLight(std::function<void(const ::eyelib::render::PointLight&)> cb) const override;
    void ForEachSpotLight(std::function<void(const ::eyelib::render::SpotLight&)> cb) const override;
    const ::eyelib::render::RenderScene* GetSceneForCallback() const override { return nullptr; }
private:
    const RenderScene& m_scene;
};

} // namespace api
} // namespace eyelib
