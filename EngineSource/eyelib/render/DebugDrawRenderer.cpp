#include "eyelib/render/DebugDrawRenderer.h"

#include "eyelib/debug/DebugDraw.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/render/2d/Font.h"
#include "eyelib/render/2d/TextRenderer.h"
#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/PassPushConstants.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/debug/Validation.h"
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace eyelib {
namespace render {

namespace {
struct LineVertex {
    math::Vec3 position;
    math::Vec4 color;
};

static rhi::VertexLayout GetLineLayout() {
    rhi::VertexLayout layout;
    layout.strideInBytes = sizeof(LineVertex);
    layout.attributes = {
        {0, rhi::VertexAttributeType::Float3, static_cast<uint32_t>(offsetof(LineVertex, position))},
        {1, rhi::VertexAttributeType::Float4, static_cast<uint32_t>(offsetof(LineVertex, color))},
    };
    return layout;
}

struct PerDeviceResources {
    std::shared_ptr<Shader> shader;
    rhi::IBuffer* vertexBufferRHI = nullptr;
    size_t vertexCapacity = 0;
    bool valid = false;
};
static std::unordered_map<rhi::IDevice*, PerDeviceResources> s_perDevice;
static bool s_shaderLoadLogged = false;

static TextRenderer s_textRenderer;
static Font s_font;
static bool s_fontReady = false;
static bool s_fontAttempted = false;
static float s_textSize = 16.0f;
static std::string s_fontPath;

static PerDeviceResources& EnsureResources(rhi::IDevice* device) {
    auto it = s_perDevice.find(device);
    if (it != s_perDevice.end() && it->second.valid) return it->second;
    PerDeviceResources res;
    res.shader = std::make_shared<Shader>();
    if (device && res.shader->Load(device, "debug/lines", GetLineLayout(), rhi::PrimitiveTopology::LineList, rhi::PipelineLayoutKind::PushOnly,
                                   std::optional{rhi::StandardAttachmentFormats::ForwardColorDepth()})) {
        res.valid = true;
    } else {
        if (!s_shaderLoadLogged) {
            core::Log::Warn("DebugDraw: failed to load debug line shader (assets/shaders/debug/lines.*). Run from project root or set working directory. Skeleton/debug lines will not be visible.");
            s_shaderLoadLogged = true;
        }
    }
    auto inserted = s_perDevice.insert({device, std::move(res)});
    return inserted.first->second;
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

void DebugDrawRenderer::Shutdown() {
    for (auto& kv : s_perDevice) {
        rhi::IDevice* dev = kv.first;
        PerDeviceResources& res = kv.second;
        if (dev && res.vertexBufferRHI) {
            dev->DestroyBuffer(res.vertexBufferRHI);
            res.vertexBufferRHI = nullptr;
        }
        res.vertexCapacity = 0;
        res.shader.reset();
        res.valid = false;
    }
    s_perDevice.clear();
}

void DebugDrawRenderer::Render(const RenderContext& ctx, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height) {
    Render(ctx.device, camera, target, width, height);
}

void DebugDrawRenderer::Render(rhi::IDevice* device, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height) {
    const auto lines = debug::DebugDraw::GetLines();
    const auto texts = debug::DebugDraw::GetTexts();
    if (lines.empty() && texts.empty()) return;
    if (width == 0 || height == 0 || !device) return;

    s_textRenderer.SetDevice(device);
    PerDeviceResources& res = EnsureResources(device);
    if (!res.shader || !res.shader->IsValid()) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
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
        if (!res.vertexBufferRHI || vertices.size() * sizeof(LineVertex) > res.vertexCapacity) {
            if (res.vertexBufferRHI) device->DestroyBuffer(res.vertexBufferRHI);
            rhi::BufferDesc desc;
            desc.sizeInBytes = std::max<size_t>(vertices.size() * sizeof(LineVertex), 1024);
            desc.usage = rhi::BufferUsage::Vertex;
            desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
            res.vertexBufferRHI = device->CreateBuffer(desc);
            res.vertexCapacity = desc.sizeInBytes;
        }
        if (!res.vertexBufferRHI) return;
        res.vertexBufferRHI->Update(vertices.data(), 0, vertices.size() * sizeof(LineVertex));

        cmd->SetDepthTest(false);
        cmd->SetDepthWrite(false);
        cmd->SetBlend(true);
        cmd->SetCullFace(false);
        res.shader->Bind(device);
        if (backend::IsVulkan(device)) {
            abi::WriteViewProjectionPairPushConstants(cmd, camera.view, camera.projection);
        } else {
            res.shader->SetMat4("u_View", camera.view);
            res.shader->SetMat4("u_Projection", camera.projection);
        }
        cmd->BindVertexBuffer(res.vertexBufferRHI, 0);
        cmd->Draw(static_cast<uint32_t>(vertices.size()), 0);
        res.shader->Unbind();
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

void DebugDrawRenderer::RenderLinesWithDepth(const RenderContext& ctx, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
    const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, const math::Vec4& color) {
    RenderLinesWithDepth(ctx.device, camera, target, width, height, lines, color);
}

void DebugDrawRenderer::RenderLinesWithDepth(rhi::IDevice* device, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
    const std::vector<std::pair<math::Vec3, math::Vec3>>& lines, const math::Vec4& color) {
    if (lines.empty() || width == 0 || height == 0 || !device) return;
    PerDeviceResources& res = EnsureResources(device);
    if (!res.shader || !res.shader->IsValid()) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd || !target) return;

    std::vector<LineVertex> vertices;
    vertices.reserve(lines.size() * 2);
    for (const auto& l : lines) {
        vertices.push_back({ l.first, color });
        vertices.push_back({ l.second, color });
    }
    if (!res.vertexBufferRHI || vertices.size() * sizeof(LineVertex) > res.vertexCapacity) {
        if (res.vertexBufferRHI) device->DestroyBuffer(res.vertexBufferRHI);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(vertices.size() * sizeof(LineVertex), 1024);
        desc.usage = rhi::BufferUsage::Vertex;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        res.vertexBufferRHI = device->CreateBuffer(desc);
        res.vertexCapacity = desc.sizeInBytes;
    }
    if (!res.vertexBufferRHI) return;
    res.vertexBufferRHI->Update(vertices.data(), 0, vertices.size() * sizeof(LineVertex));

    cmd->SetRenderTarget(target);
    cmd->SetViewport(0, 0, width, height);
    cmd->SetDepthTest(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetDepthWrite(false);
    cmd->SetBlend(false);
    cmd->SetCullFace(false);
    res.shader->Bind(device);
    if (backend::IsVulkan(device)) {
        abi::WriteViewProjectionPairPushConstants(cmd, camera.view, camera.projection);
    } else {
        res.shader->SetMat4("u_View", camera.view);
        res.shader->SetMat4("u_Projection", camera.projection);
    }
    cmd->BindVertexBuffer(res.vertexBufferRHI, 0);
    cmd->Draw(static_cast<uint32_t>(vertices.size()), 0);
    res.shader->Unbind();
}

void DebugDrawRenderer::RenderLinesWithDepth(const RenderContext& ctx, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
    const std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>>& linesWithColors) {
    RenderLinesWithDepth(ctx.device, camera, target, width, height, linesWithColors);
}

void DebugDrawRenderer::RenderLinesWithDepth(rhi::IDevice* device, const CameraView& camera, rhi::IRenderTarget* target, uint32_t width, uint32_t height,
    const std::vector<std::pair<std::pair<math::Vec3, math::Vec3>, math::Vec4>>& linesWithColors) {
    if (linesWithColors.empty() || width == 0 || height == 0 || !device) return;
    PerDeviceResources& res = EnsureResources(device);
    if (!res.shader || !res.shader->IsValid()) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (!cmd || !target) return;

    std::vector<LineVertex> vertices;
    vertices.reserve(linesWithColors.size() * 2);
    for (const auto& lc : linesWithColors) {
        const math::Vec4& color = lc.second;
        vertices.push_back({ lc.first.first, color });
        vertices.push_back({ lc.first.second, color });
    }
    if (!res.vertexBufferRHI || vertices.size() * sizeof(LineVertex) > res.vertexCapacity) {
        if (res.vertexBufferRHI) device->DestroyBuffer(res.vertexBufferRHI);
        rhi::BufferDesc desc;
        desc.sizeInBytes = std::max<size_t>(vertices.size() * sizeof(LineVertex), 1024);
        desc.usage = rhi::BufferUsage::Vertex;
        desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
        res.vertexBufferRHI = device->CreateBuffer(desc);
        res.vertexCapacity = desc.sizeInBytes;
    }
    if (!res.vertexBufferRHI) return;
    res.vertexBufferRHI->Update(vertices.data(), 0, vertices.size() * sizeof(LineVertex));

    cmd->SetRenderTarget(target);
    cmd->SetViewport(0, 0, width, height);
    cmd->SetDepthTest(true);
    cmd->SetDepthFuncLess(true);
    cmd->SetDepthWrite(false);
    cmd->SetBlend(false);
    cmd->SetCullFace(false);
    res.shader->Bind(device);
    if (backend::IsVulkan(device)) {
        abi::WriteViewProjectionPairPushConstants(cmd, camera.view, camera.projection);
    } else {
        res.shader->SetMat4("u_View", camera.view);
        res.shader->SetMat4("u_Projection", camera.projection);
    }
    cmd->BindVertexBuffer(res.vertexBufferRHI, 0);
    cmd->Draw(static_cast<uint32_t>(vertices.size()), 0);
    res.shader->Unbind();
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
