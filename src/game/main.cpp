#include "core/window/Window.h"
#include "core/log/Log.h"
#include "core/time/Time.h"
#include "core/input/Input.h"

using namespace OpenEye::Core;

int main() {
    // Initialize logging
    Log::Initialize("game.log");
    Log::SetLevel(LogLevel::Info);
    OE_INFO("=== OpenEye Game Starting ===");
    
    // Initialize window
    Window window;
    WindowConfig config;
    config.title = "OpenEye";
    config.width = 1920;
    config.height = 1080;
    config.vsync = true;
    config.debugContext = false; // Disable in release
    
    if (!window.Initialize(config)) {
        OE_FATAL("Failed to initialize window");
        Log::Shutdown();
        return -1;
    }
    
    // Setup input callbacks
    window.SetKeyCallback([&](int key, int scancode, int action, int mods) {
        Input::OnKeyEvent(key, scancode, action, mods);
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            window.SetShouldClose(true);
        }
    });
    
    window.SetMouseButtonCallback([&](int button, int action, int mods) {
        Input::OnMouseButtonEvent(button, action, mods);
    });
    
    window.SetMouseMoveCallback([&](double x, double y) {
        Input::OnMouseMoveEvent(x, y);
    });
    
    window.SetScrollCallback([&](double xoffset, double yoffset) {
        Input::OnScrollEvent(xoffset, yoffset);
    });
    
    // Initialize time
    Time::Initialize();
    
    OE_INFO("Game initialized successfully");
    OE_INFO("Press ESC to exit");
    
    // TODO: Load scene (Phase 9)
    // TODO: Initialize game systems (Phase 8+)
    
    // Main loop
    while (!window.ShouldClose()) {
        Time::Update();
        Input::Update();
        
        window.PollEvents();
        
        // Clear screen
        window.Clear(0.0f, 0.0f, 0.0f, 1.0f);
        
        // TODO: Update game logic (Phase 8+)
        // TODO: Render scene (Phase 1+)
        
        // Swap buffers
        window.SwapBuffers();
    }
    
    OE_INFO("=== OpenEye Game Shutting Down ===");
    window.Shutdown();
    Log::Shutdown();
    
    return 0;
}

