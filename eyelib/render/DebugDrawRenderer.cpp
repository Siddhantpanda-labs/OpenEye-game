#include "eyelib/render/DebugDrawRenderer.h"

#include "eyelib/debug/DebugDraw.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/render/2d/Font.h"
#include "eyelib/render/2d/TextRenderer.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/debug/Validation.h"
#include "eyelib/render/rhi/RHIContext.h"
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <algorithm>

namespace eyelib {
namespace render {

namespace {
struct LineVertex {
    math::Vec3 position;
    math::Vec4 color;
};

static std::shared_ptr<Shader> s_shader;
static TextRenderer s_textRenderer;
static Font s_font;
static bool s_fontReady = false;
static bool s_fontAttempted = false;
static float s_textSize = 16.0f;
static std::string s_fontPath;
static bool s_initialized = false;
static bool s_shaderLoadLogged = false;
static rhi::IDevice* s_deviceRHI = nullptr;
static rhi::IBuffer* s_vertexBufferRHI = nullptr;
static size_t s_vertexCapacity = 0;

static rhi::VertexLayout GetLineLayout() {
    rhi::VertexLayout layout;
    layout.strideInBytes = sizeof(LineVertex);
    layout.attributes = {
        {0, rhi::VertexAttributeType::Float3, static_cast<uint32_t>(offsetof(LineVertex, position))},
        {1, rhi::VertexAttributeType::Float4, static_cast<uint32_t>(offsetof(LineVertex, color))},
    };
    return layout;
}

static void EnsureResources() {
    if (s_initialized) return;
    s_initialized = true;
    s_deviceRHI = rhi::GetDefaultDevice();

    s_shader = std::make_shared<Shader>();
    if (!s_deviceRHI || !s_shader->LoadFromFile(s_deviceRHI, "assets/shaders/debug/lines.vert", "assets/shaders/debug/lines.frag",
                                                GetLineLayout(), rhi::PrimitiveTopology::LineList)) {
        if (!s_shaderLoadLogged) {
            core::Log::Warn("DebugDraw: failed to load debug line shader (assets/shaders/debug/lines.*). Run from project root or set working directory. Skeleton/debug lines will not be visible.");
            s_shaderLoadLogged = true;
        }
    }
}

static void TryLoadFont() {
    if (s_fontReady || s_fontAttempted) return;
    s_fontAttempted = true;

    std::vector<std::string> candidates;
    if (!s_fontPath.empty()) {
        candidates.push_back(s_fontPath);
    }
    candidates.emplace_back("assets/fonts/Roboto-Regular.ttf");
    candidates.emplace_back("assets/fonts/Inter-Regular.ttf");
    candidates.emplace_back("assets/fonts/Debug.ttf");
#if defined(_WIN32)
    candidates.emplace_back("C:/Windows/Fonts/arial.ttf");
    candidates.emplace_back("C:/Windows/Fonts/segoeui.ttf");
#endif

    std::string chosen;
    for (const auto& path : candidates) {
        if (platform::filesystem::FileExists(path)) {
            chosen = path;
            break;
        }
    }
    if (chosen.empty()) return;

    if (!s_font.LoadFromFile(chosen)) return;
    if (!s_font.BuildAtlas(s_textSize)) return;
    s_fontReady = true;
}
} // namespace

void DebugDrawRenderer::Render(const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height) {
    const auto lines = debug::DebugDraw::GetLines();
    const auto texts = debug::DebugDraw::GetTexts();
    if (lines.empty() && texts.empty()) return;
    if (width == 0 || height == 0) return;

    EnsureResources();
    if (!s_shader || !s_shader->IsValid()) return;
    if (!s_deviceRHI) return;
    rhi::ICommandList* cmd = s_deviceRHI->GetImmediateCommandList();
    if (!cmd) return;

    cmd->SetRenderTarget(target);
    cmd->SetViewport(0, 0, width, height);

    if (!lines.empty()) {
        std::vector<LineVertex> vertices;
        vertices.reserve(lines.size() * 2);
        for (const auto& l : lines) {
            vertices.push_back({ l.a, l.color });
            vertices.push_back({ l.b, l.color });
        }
        if (!s_vertexBufferRHI || vertices.size() * sizeof(LineVertex) > s_vertexCapacity) {
            if (s_vertexBufferRHI) s_deviceRHI->DestroyBuffer(s_vertexBufferRHI);
            rhi::BufferDesc desc;
            desc.sizeInBytes = std::max<size_t>(vertices.size() * sizeof(LineVertex), 1024);
            desc.usage = rhi::BufferUsage::Vertex;
            desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
            s_vertexBufferRHI = s_deviceRHI->CreateBuffer(desc);
            s_vertexCapacity = desc.sizeInBytes;
        }
        if (!s_vertexBufferRHI) return;
        s_vertexBufferRHI->Update(vertices.data(), 0, vertices.size() * sizeof(LineVertex));

        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetBlend(true);
        cmd->SetCullFace(false);
        s_shader->Bind();
        s_shader->SetMat4("u_View", camera.view);
        s_shader->SetMat4("u_Projection", camera.projection);
        cmd->BindVertexBuffer(s_vertexBufferRHI, 0);
        cmd->Draw(static_cast<uint32_t>(vertices.size()), 0);
        s_shader->Unbind();
    }

    if (!texts.empty()) {
        TryLoadFont();
        if (s_fontReady) {
            math::Mat4 view = math::Mat4(1.0f);
            math::Mat4 proj = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height));
            const math::Mat4 vp = camera.projection * camera.view;
            for (const auto& t : texts) {
                math::Vec4 clip = vp * math::Vec4(t.position, 1.0f);
                if (clip.w <= 0.0f) continue;
                math::Vec3 ndc = math::Vec3(clip) / clip.w;
                if (ndc.z < -1.0f || ndc.z > 1.0f) continue;
                float sx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(width);
                float sy = (ndc.y * 0.5f + 0.5f) * static_cast<float>(height);
                s_textRenderer.RenderText(t.text, s_font, math::Vec2(sx, sy), t.color, s_textSize, view, proj);
            }
        } else {
            debug::Validation::Report(debug::ValidationSeverity::Warning,
                "DebugDraw text requested but no font was found",
                __FILE__, __LINE__);
        }
    }
}

void DebugDrawRenderer::SetTextFontPath(const std::string& path, float pixelHeight) {
    s_fontPath = path;
    s_textSize = pixelHeight;
    s_fontReady = false;
    s_fontAttempted = false;
}

void DebugDrawRenderer::SetTextSize(float pixelHeight) {
    s_textSize = pixelHeight;
    s_fontReady = false;
    s_fontAttempted = false;
}

} // namespace render
} // namespace eyelib
