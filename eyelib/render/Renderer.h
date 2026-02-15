#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"

namespace eyelib {
namespace render {

/**
 * @brief Minimal renderer for early bring-up.
 *
 * This is intentionally small: clear + draw mesh with shader.
 * We’ll grow it into a proper pipeline/queues/framegraph next.
 */
class Renderer {
public:
    bool Initialize();
    void Shutdown();

    void Clear(float r, float g, float b, float a);
    void Draw(const Mesh& mesh, const Shader& shader);

    void SetDevice(rhi::IDevice* device) { m_injectedDevice = device; }

private:
    rhi::IDevice* m_injectedDevice = nullptr;
    bool m_initialized = false;
};

} // namespace render
} // namespace eyelib

