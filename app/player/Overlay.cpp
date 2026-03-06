#include "Overlay.h"

#include "api/core/All.h"

#ifdef OPENEYE_BUILD_STREAMING
#include <glad/glad.h>
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#ifdef OPENEYE_BUILD_STREAMING
#include <qrcodegen.hpp>
#include <vector>
#endif

namespace openeye::player {

bool RuntimeOverlay::Initialize(eyelib::api::Window& window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(window.GetNativeHandle()), true);
    ImGui_ImplOpenGL3_Init("#version 450");
    m_ready = true;
    return true;
}

void RuntimeOverlay::Render(const OverlaySnapshot& snapshot) {
    if (!m_ready) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav;
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("Runtime Overlay", nullptr, flags)) {
        ImGui::Text("Scene: %s", snapshot.scenePath.c_str());
        ImGui::Text("FPS: %.1f", snapshot.fps);
        ImGui::Text("Frame: %.3f ms", snapshot.frameTimeMs);
        ImGui::Text("Entities: %zu", snapshot.entityCount);
    }
    ImGui::End();

    if (snapshot.streamingActive && !snapshot.streamingServerAddress.empty()) {
        const std::string uri = "openeye://" + snapshot.streamingServerAddress;
#ifdef OPENEYE_BUILD_STREAMING
        if (uri != m_lastQRAddress) {
            if (m_qrTextureId) {
                glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_qrTextureId));
                m_qrTextureId = nullptr;
            }
            try {
                const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(uri.c_str(), qrcodegen::QrCode::Ecc::LOW);
                const int size = qr.getSize();
                const int scale = 8;
                const int w = size * scale;
                const int h = size * scale;
                std::vector<uint8_t> rgba(static_cast<size_t>(w * h * 4));
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++) {
                        const bool dark = qr.getModule(x / scale, y / scale);
                        const size_t i = static_cast<size_t>((y * w + x) * 4);
                        rgba[i + 0] = dark ? 0 : 255;
                        rgba[i + 1] = dark ? 0 : 255;
                        rgba[i + 2] = dark ? 0 : 255;
                        rgba[i + 3] = 255;
                    }
                }
                GLuint texId = 0;
                glGenTextures(1, &texId);
                glBindTexture(GL_TEXTURE_2D, texId);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                glBindTexture(GL_TEXTURE_2D, 0);
                m_qrTextureId = reinterpret_cast<void*>(static_cast<uintptr_t>(texId));
                m_qrTextureWidth = w;
                m_qrTextureHeight = h;
                m_lastQRAddress = uri;
            } catch (...) {
                m_lastQRAddress.clear();
            }
        }
        if (m_qrTextureId && m_qrTextureWidth > 0 && m_qrTextureHeight > 0) {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.9f);
            if (ImGui::Begin("OpenEye VR Server", nullptr, flags)) {
                ImGui::Text("Device connected: %zu", snapshot.streamingClientCount);
                ImGui::Separator();
                ImGui::Image(m_qrTextureId, ImVec2(static_cast<float>(m_qrTextureWidth), static_cast<float>(m_qrTextureHeight)));
                ImGui::Spacing();
                ImGui::TextUnformatted("openeye://");
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(snapshot.streamingServerAddress.c_str());
                ImGui::Spacing();
                ImGui::TextUnformatted("Scan with app or enter IP:port");
            }
            ImGui::End();
        } else
#endif
        {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.9f);
            if (ImGui::Begin("OpenEye VR Server", nullptr, flags)) {
                ImGui::Text("Device connected: %zu", snapshot.streamingClientCount);
                ImGui::Separator();
                ImGui::TextUnformatted("openeye://");
                ImGui::SameLine(0, 0);
                ImGui::TextUnformatted(snapshot.streamingServerAddress.c_str());
                ImGui::Spacing();
                ImGui::TextUnformatted("Scan QR or enter IP:port in app");
            }
            ImGui::End();
        }
    } else {
#ifdef OPENEYE_BUILD_STREAMING
        if (m_qrTextureId) {
            glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_qrTextureId));
            m_qrTextureId = nullptr;
            m_qrTextureWidth = 0;
            m_qrTextureHeight = 0;
            m_lastQRAddress.clear();
        }
#endif
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void RuntimeOverlay::Shutdown() {
    if (!m_ready) {
        return;
    }
#ifdef OPENEYE_BUILD_STREAMING
    if (m_qrTextureId) {
        glDeleteTextures(1, reinterpret_cast<GLuint*>(&m_qrTextureId));
        m_qrTextureId = nullptr;
        m_qrTextureWidth = 0;
        m_qrTextureHeight = 0;
        m_lastQRAddress.clear();
    }
#endif
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_ready = false;
}

} // namespace openeye::player
