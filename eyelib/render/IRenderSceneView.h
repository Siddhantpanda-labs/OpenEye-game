#pragma once

#include "eyelib/render/RenderTypes.h"
#include <functional>
#include <memory>

namespace eyelib {
namespace render {

class TextureCube;

/** Read-only view over scene data. Pipeline uses this to avoid copying; API and ECS both implement it. */
class IRenderSceneView {
public:
    virtual ~IRenderSceneView() = default;

    virtual CameraView GetCamera() const = 0;
    virtual LightParams GetLight() const = 0;
    virtual std::shared_ptr<TextureCube> GetSkybox() const = 0;

    virtual void ForEachOpaqueDrawable(std::function<void(const Drawable&)> cb) const = 0;
    virtual void ForEachTransparentDrawable(std::function<void(const Drawable&)> cb) const = 0;
    virtual void ForEachPointLight(std::function<void(const PointLight&)> cb) const = 0;
    virtual void ForEachSpotLight(std::function<void(const SpotLight&)> cb) const = 0;

    /** Optional: for PrePostCallback that expects RenderScene. Returns nullptr if view is over API scene. */
    virtual const RenderScene* GetSceneForCallback() const { return nullptr; }
};

/** Wraps RenderScene as a view (zero copy). */
class RenderSceneView : public IRenderSceneView {
public:
    explicit RenderSceneView(const RenderScene& scene) : m_scene(scene) {}

    CameraView GetCamera() const override { return m_scene.camera; }
    LightParams GetLight() const override { return m_scene.light; }
    std::shared_ptr<TextureCube> GetSkybox() const override { return m_scene.skybox; }

    void ForEachOpaqueDrawable(std::function<void(const Drawable&)> cb) const override {
        for (const auto& d : m_scene.opaque) cb(d);
    }
    void ForEachTransparentDrawable(std::function<void(const Drawable&)> cb) const override {
        for (const auto& d : m_scene.transparent) cb(d);
    }
    void ForEachPointLight(std::function<void(const PointLight&)> cb) const override {
        for (const auto& p : m_scene.pointLights) cb(p);
    }
    void ForEachSpotLight(std::function<void(const SpotLight&)> cb) const override {
        for (const auto& s : m_scene.spotLights) cb(s);
    }

    const RenderScene* GetSceneForCallback() const override { return &m_scene; }

private:
    const RenderScene& m_scene;
};

} // namespace render
} // namespace eyelib
