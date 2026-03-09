/*
    Metaverse Game - VR-Ready Application
    Based on the working RendererApp with terrain, skybox, and raytracing
*/
#include <iostream>
#include <memory>
#include <chrono>
#include <filesystem>
#include <thread>
#include <iomanip>
#include <atomic>
#include <mutex>

// Engine includes
#include <engine/core.h>
#include <engine/systems/renderers/forward.h>
#include <engine/tools/controller.h>
#include <engine/tools/loaders.h>
#include <engine/tools/gui.h>
#include <engine/tools/widgets.h>
#include <GLFW/glfw3.h>

// VR Streaming includes - UDP with GameNetworkingSockets
#include "networking/VRStreamingManager.h"

// ImGui includes (simplified for now)
// #include <imgui.h>
// #include <backends/imgui_impl_glfw.h>
// #include <backends/imgui_impl_vulkan.h>

VULKAN_ENGINE_NAMESPACE_BEGIN

class MetaverseGame {
private:
    ptr<IWindow> m_window;
    ptr<Systems::BaseRenderer> m_renderer;
    Scene* m_scene;
    Camera* m_camera;
    Tools::Controller* m_controller;
    
    // VR Streaming - UDP with GameNetworkingSockets
    std::unique_ptr<App::Metaworld::VRStreamingManager> m_vrStreaming;
    
    // Direct streaming (no multi-threading needed)
    bool m_directStreaming = false;
    
    // Timing
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    const double m_targetFrameTime = 1.0 / 60.0; // 60 FPS target
    double m_accumulatedTime = 0.0;
    
    // Animation
    bool m_animateLight = true;
    Mesh* m_lightDummy = nullptr;
    
    // VR Streaming state
    bool m_rendererConnected = false;
    
    // GUI Interface
    struct {
        Tools::GUIOverlay* overlay = nullptr;
        Tools::Panel* clientPanel = nullptr;
    } m_interface;

public:
    MetaverseGame() {
        std::cout << "🎮 Metaverse Game - VR-Ready Application" << std::endl;
        m_lastFrameTime = std::chrono::high_resolution_clock::now();
    }
    
    ~MetaverseGame() {
        std::cout << "🛑 Shutting down Metaverse Game..." << std::endl;
    }
    
    void initialize() {
        std::cout << "🚀 Initializing Metaverse Game..." << std::endl;
        
        // Initialize window
        m_window = std::make_shared<WindowGLFW>("Metaverse - VR Ready", 1280, 720);
        m_window->init();
        
        // Disable VSync for higher FPS
        void* windowHandle = nullptr;
        m_window->get_handle(windowHandle);
        GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(windowHandle);
        glfwSwapInterval(0); // Disable VSync for 60+ FPS
        
        // Set up callbacks
        m_window->set_window_size_callback(std::bind(&MetaverseGame::window_resize_callback, this, std::placeholders::_1, std::placeholders::_2));
        m_window->set_mouse_callback(std::bind(&MetaverseGame::mouse_callback, this, std::placeholders::_1, std::placeholders::_2));
        m_window->set_key_callback(std::bind(&MetaverseGame::keyboard_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
        
        // Initialize renderer with optimized settings for 60 FPS
        Systems::RendererSettings settings{};
        settings.samplesMSAA = MSAASamples::x1; // Disable MSAA for better performance
        settings.clearColor = Vec4(0.02f, 0.02f, 0.02f, 1.0f);
        settings.enableUI = true; // Enable UI for client overlay
        settings.enableRaytracing = true; // Enable raytracing
        settings.softwareAA = SoftwareAA::FXAA; // Keep FXAA for quality
        
        m_renderer = std::make_shared<Systems::ForwardRenderer>(m_window, settings);
        
        // Setup scene
        setupScene();
        
        // Setup GUI
        setupGUI();
        
        std::cout << "✅ Metaverse Game initialized successfully!" << std::endl;
    }
    
    void setupGUI() {
        std::cout << "🎨 Setting up VR Client Overlay..." << std::endl;
        
        // Create GUI overlay
        m_interface.overlay = new Tools::GUIOverlay(
            (float)m_window->get_extent().width, 
            (float)m_window->get_extent().height, 
            GuiColorProfileType::DARK
        );
        
        // Create VR Client Status Panel
        m_interface.clientPanel = new Tools::Panel(
            "VR STREAMING SERVER", 
            0.02f, 0.02f, 0.35f, 0.4f, 
            PanelWidgetFlags::NoMove, 
            false, 
            true
        );
        
        // Add server status info
        m_interface.clientPanel->add_child(new Tools::Space());
        m_interface.clientPanel->add_child(new Tools::Separator("SERVER STATUS"));
        m_interface.clientPanel->add_child(new Tools::Separator());
        m_interface.clientPanel->add_child(new Tools::TextLine("🟢 VR Server: ACTIVE", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("Mode: DIRECT STREAMING", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("Port: 27015", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("Resolution: 1920x1080", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("Target FPS: 60", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::Space());
        
        // Add client info section
        m_interface.clientPanel->add_child(new Tools::Separator("CONNECTED CLIENTS"));
        m_interface.clientPanel->add_child(new Tools::Separator());
        m_interface.clientPanel->add_child(new Tools::TextLine("⏳ Waiting for clients...", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("Clients can discover this server", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("automatically on the same WiFi", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::Space());
        
        // Add controls info
        m_interface.clientPanel->add_child(new Tools::Separator("CONTROLS"));
        m_interface.clientPanel->add_child(new Tools::Separator());
        m_interface.clientPanel->add_child(new Tools::TextLine("WASD: Move camera", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("Mouse: Look around", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("L: Toggle light animation", TextWidgetType::BULLET));
        m_interface.clientPanel->add_child(new Tools::TextLine("ESC: Exit application", TextWidgetType::BULLET));
        
        // Add the panel to overlay
        m_interface.overlay->add_panel(m_interface.clientPanel);
        
        std::cout << "✅ VR Client Overlay setup complete!" << std::endl;
    }
    
    bool setupScene() {
        std::cout << "🌍 Setting up VR-ready scene..." << std::endl;
        
        
        const std::string MESH_PATH(EXAMPLES_RESOURCES_PATH "meshes/");
        const std::string TEXTURE_PATH(EXAMPLES_RESOURCES_PATH "textures/");
        const std::string ENGINE_MESH_PATH(ENGINE_RESOURCES_PATH "meshes/");
        
        // Create camera
        m_camera = new Camera();
        m_camera->set_position(Vec3(0.0f, 2.0f, 0.0f)); // VR height
        m_camera->set_far(1000.0f);
        m_camera->set_near(0.1f);
        m_camera->set_field_of_view(70.0f);
        
        m_scene = new Scene(m_camera);
        
        // Create lighting setup
        setupLighting();
        
        // Create terrain
        createTerrain();
        
        // Create buildings and props
        createBuildings();
        
        // Create skybox
        createSkybox();
        
        // Create controller
        m_controller = new Tools::Controller(m_camera, m_window.get());
        
        // Initialize VR streaming - UDP with GameNetworkingSockets
        m_vrStreaming = std::make_unique<App::Metaworld::VRStreamingManager>();
        if (!m_vrStreaming->initialize(27016, 1920, 1080)) { // Use streaming port
            std::cout << "⚠️ Failed to initialize VR streaming, continuing without it..." << std::endl;
        } else {
            // Set up gyroscope callback for phone input
            m_vrStreaming->setGyroscopeCallback([this](float x, float y, float z) {
                handleGyroscopeInput(x, y, z);
            });
            
            // Configure streaming settings - DIRECT STREAMING OPTIMIZED
            m_vrStreaming->setStreamingQuality(80); // Higher quality for direct streaming
            m_vrStreaming->setMaxFPS(60); // 60 FPS for direct streaming
            
            std::cout << "🔗 VR streaming ready - direct streaming mode enabled!" << std::endl;
            // Don't connect immediately - wait for first successful render
        }

        
        std::cout << "✅ VR-ready scene created!" << std::endl;
        return true;
    }
    
    void setupLighting() {
        std::cout << "💡 Setting up lighting..." << std::endl;
        
        // Main directional light (sun)
        m_scene->add(new PointLight());
        m_scene->get_lights()[0]->set_position({-3.0f, 10.0f, 0.0f});
        m_scene->get_lights()[0]->set_shadow_fov(115.0f);
        m_scene->get_lights()[0]->set_color({1.0f, 0.95f, 0.8f});
        m_scene->get_lights()[0]->set_intensity(2.0f);
        
        // Additional point lights for atmosphere
        PointLight* light1 = new PointLight();
        light1->set_area_of_effect(25.0f);
        m_scene->add(light1);
        m_scene->get_lights()[1]->set_position({10.0f, 5.0f, 10.0f});
        m_scene->get_lights()[1]->set_color({0.0f, 0.3f, 0.8f});
        m_scene->get_lights()[1]->set_intensity(1.5f);
        
        PointLight* light2 = new PointLight();
        light2->set_area_of_effect(15.0f);
        m_scene->add(light2);
        m_scene->get_lights()[2]->set_position({-10.0f, 3.0f, 5.0f});
        m_scene->get_lights()[2]->set_color({0.8f, 0.3f, 0.0f});
        m_scene->get_lights()[2]->set_intensity(1.0f);
        
        // Set ambient lighting
        m_scene->set_ambient_color({0.1f, 0.15f, 0.3f});
        
        std::cout << "✅ Lighting setup complete!" << std::endl;
    }
    
    void createTerrain() {
        std::cout << "🏔️ Creating terrain..." << std::endl;
        
        const std::string MESH_PATH(EXAMPLES_RESOURCES_PATH "meshes/");
        const std::string TEXTURE_PATH(EXAMPLES_RESOURCES_PATH "textures/");
        
        // Create terrain mesh
        Mesh* terrainMesh = new Mesh();
        terrainMesh->set_scale(20.0f); // Large terrain
        terrainMesh->set_position({0.0f, -5.0f, 0.0f});
        Tools::Loaders::load_3D_file(terrainMesh, MESH_PATH + "terrain.obj");
        
        // Create terrain material
        auto terrainMat = new PhysicalMaterial();
        TextureLDR* floorText = new TextureLDR();
        Tools::Loaders::load_texture(floorText, TEXTURE_PATH + "floor_diffuse.jpg");
        TextureLDR* floorNormalText = new TextureLDR();
        Tools::Loaders::load_texture(floorNormalText, TEXTURE_PATH + "floor_normal.jpg", TEXTURE_FORMAT_UNORM);
        TextureLDR* floorRoughText = new TextureLDR();
        Tools::Loaders::load_texture(floorRoughText, TEXTURE_PATH + "floor_roughness.jpg");
        
        terrainMat->set_albedo({0.43f, 0.28f, 0.23f});
        terrainMat->set_albedo_texture(floorText);
        terrainMat->set_normal_texture(floorNormalText);
        terrainMat->set_roughness_texture(floorRoughText);
        terrainMat->set_tile({25.0f, 25.0f});
        terrainMat->set_metalness(0.1f);
        terrainMat->set_roughness(0.8f);
        
        terrainMesh->add_material(terrainMat);
        terrainMesh->set_name("Terrain");
        terrainMesh->ray_hittable(true); // Explicitly enable raytracing
        m_scene->add(terrainMesh);
        
        std::cout << "✅ Terrain created!" << std::endl;
    }
    
    void createBuildings() {
        std::cout << "🏢 Creating buildings..." << std::endl;
        
        const std::string MESH_PATH(EXAMPLES_RESOURCES_PATH "meshes/");
        const std::string TEXTURE_PATH(EXAMPLES_RESOURCES_PATH "textures/");
        
        // Create temple building
        Mesh* templeMesh = new Mesh();
        Tools::Loaders::load_3D_file(templeMesh, MESH_PATH + "temple.obj");
        templeMesh->set_rotation({0.0f, 180.0f, 0.0f});
        
        auto templeMat = new PhysicalMaterial();
        TextureLDR* templeText = new TextureLDR();
        Tools::Loaders::load_texture(templeText, TEXTURE_PATH + "temple_diffuse.png");
        TextureLDR* templeRText = new TextureLDR();
        Tools::Loaders::load_texture(templeRText, TEXTURE_PATH + "temple_rough.png");
        TextureLDR* templeMText = new TextureLDR();
        Tools::Loaders::load_texture(templeMText, TEXTURE_PATH + "temple_metal.png");
        
        templeMat->set_albedo_texture(templeText);
        templeMat->set_metallic_texture(templeMText);
        templeMat->set_roughness_texture(templeRText);
        templeMat->set_metalness(0.8f);
        templeMat->set_roughness(0.4f);
        
        templeMesh->add_material(templeMat);
        templeMesh->set_name("Temple");
        templeMesh->set_position({15.0f, -2.87f, 20.0f});
        templeMesh->set_rotation({0.0f, 230.0f, 0.0f});
        templeMesh->ray_hittable(true); // Explicitly enable raytracing
        m_scene->add(templeMesh);
        
        // Create shrine
        Mesh* shrineMesh = new Mesh();
        Tools::Loaders::load_3D_file(shrineMesh, MESH_PATH + "shrine.obj");
        
        auto shrineMat = new PhysicalMaterial();
        TextureLDR* shrineText = new TextureLDR();
        Tools::Loaders::load_texture(shrineText, TEXTURE_PATH + "shrine_diffuse.png");
        TextureLDR* shrineRText = new TextureLDR();
        Tools::Loaders::load_texture(shrineRText, TEXTURE_PATH + "shrine_rough.png");
        TextureLDR* shrineMText = new TextureLDR();
        Tools::Loaders::load_texture(shrineMText, TEXTURE_PATH + "shrine_metal.png");
        
        shrineMat->set_albedo_texture(shrineText);
        shrineMat->set_metallic_texture(shrineMText);
        shrineMat->set_roughness_texture(shrineRText);
        shrineMat->set_metalness(0.8f);
        shrineMat->set_roughness(0.4f);
        
        shrineMesh->add_material(shrineMat);
        shrineMesh->set_name("Shrine");
        shrineMesh->set_position({5.0f, -2.77f, 15.0f});
        shrineMesh->set_rotation({0.0f, 160.0f, 0.0f});
        shrineMesh->set_scale(1.25f);
        shrineMesh->ray_hittable(true); // Explicitly enable raytracing
        m_scene->add(shrineMesh);
        
        std::cout << "✅ Buildings created!" << std::endl;
    }
    
    void createSkybox() {
        std::cout << "🌌 Creating skybox..." << std::endl;
        
        const std::string TEXTURE_PATH(EXAMPLES_RESOURCES_PATH "textures/");
        
        // Create HDR environment map
        TextureHDR* envMap = new TextureHDR();
        Tools::Loaders::load_texture(envMap, TEXTURE_PATH + "cloudy.hdr");
        Skybox* sky = new Skybox(envMap);
        sky->set_color_intensity(0.25f);
        m_scene->set_skybox(sky);
        
        std::cout << "✅ Skybox created!" << std::endl;
    }
    
    
    void run() {
        std::cout << "🎮 Starting VR-ready game loop..." << std::endl;
        std::cout << "🎯 Controls: WASD = Move, Mouse = Look Around, L = Toggle Light Animation" << std::endl;
        
        while (!m_window->get_window_should_close()) {
            // Frame rate limiting for consistent 60 FPS
            m_frameStartTime = std::chrono::high_resolution_clock::now();
            
            // Calculate delta time
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - m_lastFrameTime).count();
            m_lastFrameTime = currentTime;
            
            // Accumulate time for frame rate limiting
            m_accumulatedTime += deltaTime;
            
            // Update scene
            update(deltaTime);
            
            // Handle input
            m_controller->handle_keyboard(0, 0, deltaTime);
            
            // Poll events
            m_window->poll_events();
            
            // Connect VR streaming after first successful render (when renderer is fully initialized)
            if (!m_rendererConnected && m_vrStreaming) {
                m_vrStreaming->setRenderer(m_renderer.get());
                m_vrStreaming->startStreaming();
                m_rendererConnected = true;
                std::cout << "🔗 VR streaming connected after first successful render!" << std::endl;
            }
            
            // DIRECT STREAMING: Check if VR client is connected
            bool hasVRClient = false;
            if (m_vrStreaming && m_vrStreaming->isStreaming()) {
                auto clients = m_vrStreaming->getConnectedClients();
                hasVRClient = !clients.empty();
            }
            
            if (hasVRClient) {
                // VR CLIENT CONNECTED: Direct streaming mode
                m_directStreaming = true;
                
                // Render directly to VR stream (no local display)
                m_vrStreaming->renderDirectToStream(m_scene);
                
                // Minimal GUI for VR mode (only essential info)
                if (m_interface.overlay) {
                    m_interface.overlay->render();
                }
            } else {
                // NO VR CLIENT: Normal rendering mode
                m_directStreaming = false;
                
                // Normal rendering to local display
                m_renderer->render(m_scene);
                
                // Full GUI overlay - PERFORMANCE-OPTIMIZED: Only render every 3rd frame
                static int guiFrameCounter = 0;
                if (m_interface.overlay && (guiFrameCounter % 3 == 0)) {
                    m_interface.overlay->render();
                }
                guiFrameCounter++;
            }
            
            // ADAPTIVE Frame rate limiting - only limit if FPS > 120 (too high)
            auto frameEndTime = std::chrono::high_resolution_clock::now();
            auto frameDuration = std::chrono::duration<double>(frameEndTime - m_frameStartTime).count();
            
            // Only apply frame limiting if we're running way too fast (> 120 FPS)
            if (frameDuration < (1.0 / 120.0)) {
                auto sleepTime = std::chrono::duration<double>((1.0 / 120.0) - frameDuration);
                std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::microseconds>(sleepTime));
            }
        }
        
        std::cout << "🏁 Game loop ended." << std::endl;
    }
    
    void update(float deltaTime) {
        // Animate lights if enabled
        if (m_animateLight && m_scene->get_lights().size() > 0) {
            auto light = m_scene->get_lights()[0];
            float rotationAngle = glm::radians(10.0f * deltaTime);
            float _x = light->get_position().x * cos(rotationAngle) - light->get_position().z * sin(rotationAngle);
            float _z = light->get_position().x * sin(rotationAngle) + light->get_position().z * cos(rotationAngle);
            light->set_position({_x, light->get_position().y, _z});
        }
        
        // Update client list in GUI (every 2 seconds)
        static float clientUpdateTimer = 0.0f;
        clientUpdateTimer += deltaTime;
        if (clientUpdateTimer >= 2.0f) {
            updateClientList();
            clientUpdateTimer = 0.0f;
        }
        
        // Monitor FPS and auto-adjust quality (every 2 seconds)
        static float fpsTimer = 0.0f;
        static int frameCount = 0;
        fpsTimer += deltaTime;
        frameCount++;
        
        if (fpsTimer >= 2.0f) {
            float currentFPS = frameCount / fpsTimer;
            std::cout << "📊 Current FPS: " << std::fixed << std::setprecision(1) << currentFPS << std::endl;
            
            // Auto-adjust VR streaming quality based on performance
            if (m_vrStreaming && m_vrStreaming->isStreaming()) {
                if (currentFPS < 30.0f) {
                    // Performance is poor - reduce quality
                    m_vrStreaming->setStreamingQuality(50);
                    std::cout << "⚠️ Low FPS detected - reducing VR quality to 50%" << std::endl;
                } else if (currentFPS > 80.0f) {
                    // Performance is good - increase quality
                    m_vrStreaming->setStreamingQuality(80);
                    std::cout << "✅ Good FPS - increasing VR quality to 80%" << std::endl;
                }
            }
            
            fpsTimer = 0.0f;
            frameCount = 0;
        }
    }
    
    void updateClientList() {
        if (!m_interface.clientPanel || !m_vrStreaming) return;
        
        // Get connected clients
        auto connectedClients = m_vrStreaming->getConnectedClients();
        
        // Update the client count display
        // Note: The GUI system is static, so we'll use console output for now
        // In a more advanced implementation, we could dynamically update the TextLine widgets
        if (!connectedClients.empty()) {
            std::cout << "📱 Connected VR clients: " << connectedClients.size() << " (Direct Streaming Mode)" << std::endl;
            for (size_t i = 0; i < connectedClients.size(); ++i) {
                std::cout << "   Client " << (i + 1) << ": " << connectedClients[i] << std::endl;
            }
        }
    }
    
    void handleGyroscopeInput(float x, float y, float z) {
        // Convert gyroscope data to camera rotation
        // x = pitch (up/down), y = yaw (left/right), z = roll (tilt)
        
        static float sensitivity = 0.01f;
        static float pitch = 0.0f;
        static float yaw = 0.0f;
        
        // Update camera rotation based on gyroscope
        yaw += x * sensitivity;
        pitch += y * sensitivity;
        
        // Clamp pitch to avoid over-rotation
        pitch = std::clamp(pitch, -89.0f, 89.0f);
        
        // Apply rotation to camera
        if (m_camera) {
            // Convert to camera look direction
            Vec3 direction;
            direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            direction.y = sin(glm::radians(pitch));
            direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            direction = glm::normalize(direction);
            
            // Update camera look direction
            m_camera->look_at(m_camera->get_position(), m_camera->get_position() + direction, Vec3(0.0f, 1.0f, 0.0f));
        }
    }
    
    // Direct streaming method (no threading needed)
    void handleDirectStreaming() {
        if (m_vrStreaming && m_vrStreaming->isStreaming()) {
            auto clients = m_vrStreaming->getConnectedClients();
            if (!clients.empty()) {
                // Direct streaming to VR client
                m_vrStreaming->streamDirectFrame();
            }
        }
    }
    
    void shutdown() {
        std::cout << "🛑 Shutting down..." << std::endl;
        
        
        // Shutdown VR streaming - UDP with GameNetworkingSockets
        if (m_vrStreaming) {
            m_vrStreaming->shutdown();
            m_vrStreaming.reset();
        }

        
        // Cleanup GUI
        if (m_interface.overlay) {
            delete m_interface.overlay;
            m_interface.overlay = nullptr;
        }
        
        if (m_renderer) {
            m_renderer->shutdown(m_scene);
        }
        std::cout << "✅ Shutdown complete!" << std::endl;
    }
    
    // Input callbacks
    void keyboard_callback(int key, int scancode, int action, int mods) {
        void* windowHandle{nullptr};
        m_window->get_handle(windowHandle);
        GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(windowHandle);
        
        if (glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            m_window->set_window_should_close(true);
        }
        
        if (glfwGetKey(glfwWindow, GLFW_KEY_L) == GLFW_PRESS) {
            m_animateLight = !m_animateLight;
            std::cout << "💡 Light animation: " << (m_animateLight ? "ON" : "OFF") << std::endl;
        }
    }
    
    
    void mouse_callback(double xpos, double ypos) {
        m_controller->handle_mouse((float)xpos, (float)ypos);
    }
    
    void window_resize_callback(int width, int height) {
        m_window->set_size(width, height);
        
        // Adjust VR streaming quality based on window size for consistent performance
        if (m_vrStreaming) {
            int totalPixels = width * height;
            if (totalPixels > 1920 * 1080) {
                // Large window - reduce quality for performance
                m_vrStreaming->setStreamingQuality(70);
                std::cout << "🔄 Large window detected - reducing quality to 70% for performance" << std::endl;
            } else if (totalPixels < 1280 * 720) {
                // Small window - can use higher quality
                m_vrStreaming->setStreamingQuality(85);
                std::cout << "🔄 Small window detected - increasing quality to 85%" << std::endl;
            } else {
                // Medium window - balanced quality
                m_vrStreaming->setStreamingQuality(80);
                std::cout << "🔄 Medium window detected - using balanced quality 80%" << std::endl;
            }
        }
    }
};

VULKAN_ENGINE_NAMESPACE_END

int main() {
    std::cout << "🎮 Metaverse Game - VR-Ready Application" << std::endl;
    std::cout << "🚀 Starting with terrain, skybox, and raytracing..." << std::endl;
    
    try {
        VKFW::MetaverseGame game;
        
        game.initialize();
        game.run();
        game.shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Game crashed: " << e.what() << std::endl;
        return -1;
    }
    
    std::cout << "🎉 Game completed successfully!" << std::endl;
    return 0;
}