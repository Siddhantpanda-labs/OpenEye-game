#include "api/render/RenderSceneAdapter.h"
#include "api/render/RenderScene.h"
#include "api/render/Mesh.h"
#include "api/render/Material.h"
#include "api/render/MaterialInstance.h"
#include "api/render/TextureCube.h"
#include "eyelib/render/RenderTypes.h"

namespace eyelib {
namespace api {

namespace {
::eyelib::render::Drawable ToEngineDrawable(const Drawable& d) {
    ::eyelib::render::Drawable out;
    out.mesh = d.mesh ? d.mesh->GetInternal() : nullptr;
    out.material = d.material ? d.material->GetInternal() : nullptr;
    out.materialInstance = d.materialInstance ? d.materialInstance->GetInternal() : nullptr;
    out.transform = d.transform;
    out.bones = d.bones;
    out.boneCount = d.boneCount;
    return out;
}

::eyelib::render::LightParams ToEngineLightParams(const render::DirectionalLight& L) {
    ::eyelib::render::LightParams out;
    out.direction = L.direction;
    out.color = L.color;
    out.intensity = L.intensity;
    out.ambient = L.ambient;
    out.exposure = L.exposure;
    out.castShadows = L.castShadows;
    return out;
}

::eyelib::render::PointLight ToEnginePointLight(const render::PointLight& p) {
    ::eyelib::render::PointLight out;
    out.position = p.position;
    out._pad0 = 0.0f;
    out.color = p.color;
    out.intensity = p.intensity;
    out.radius = p.radius;
    out._pad1 = out._pad2 = out._pad3 = 0.0f;
    return out;
}

::eyelib::render::SpotLight ToEngineSpotLight(const render::SpotLight& s) {
    ::eyelib::render::SpotLight out;
    out.position = s.position;
    out._pad0 = 0.0f;
    out.direction = s.direction;
    out._pad1 = 0.0f;
    out.color = s.color;
    out.intensity = s.intensity;
    out.range = s.range;
    out.innerConeCos = s.innerConeCos;
    out.outerConeCos = s.outerConeCos;
    out._pad2 = out._pad3 = out._pad4 = out._pad5 = out._pad6 = 0.0f;
    return out;
}
} // namespace

::eyelib::render::CameraView ApiRenderSceneView::GetCamera() const {
    ::eyelib::render::CameraView out;
    out.view = m_scene.camera.view;
    out.projection = m_scene.camera.projection;
    out.viewPos = m_scene.camera.viewPos;
    out.nearPlane = m_scene.camera.nearPlane;
    out.farPlane = m_scene.camera.farPlane;
    return out;
}

::eyelib::render::LightParams ApiRenderSceneView::GetLight() const {
    return ToEngineLightParams(m_scene.light);
}

std::shared_ptr<::eyelib::render::TextureCube> ApiRenderSceneView::GetSkybox() const {
    return m_scene.skybox ? m_scene.skybox->GetInternal() : nullptr;
}

void ApiRenderSceneView::ForEachOpaqueDrawable(std::function<void(const ::eyelib::render::Drawable&)> cb) const {
    if (!m_scene.opaque) return;
    for (size_t i = 0; i < m_scene.opaqueCount; ++i)
        cb(ToEngineDrawable(m_scene.opaque[i]));
}

void ApiRenderSceneView::ForEachTransparentDrawable(std::function<void(const ::eyelib::render::Drawable&)> cb) const {
    if (!m_scene.transparent) return;
    for (size_t i = 0; i < m_scene.transparentCount; ++i)
        cb(ToEngineDrawable(m_scene.transparent[i]));
}

void ApiRenderSceneView::ForEachPointLight(std::function<void(const ::eyelib::render::PointLight&)> cb) const {
    if (!m_scene.pointLights) return;
    for (size_t i = 0; i < m_scene.pointLightCount; ++i)
        cb(ToEnginePointLight(m_scene.pointLights[i]));
}

void ApiRenderSceneView::ForEachSpotLight(std::function<void(const ::eyelib::render::SpotLight&)> cb) const {
    if (!m_scene.spotLights) return;
    for (size_t i = 0; i < m_scene.spotLightCount; ++i)
        cb(ToEngineSpotLight(m_scene.spotLights[i]));
}

} // namespace api
} // namespace eyelib
