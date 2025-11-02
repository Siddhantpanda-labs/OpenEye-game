#include "core/window/Window.h"
#include "core/log/Log.h"
#include "core/time/Time.h"
#include "core/input/Input.h"
#include "core/filesystem/ShaderFileWatcher.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include "ecs/Systems.h"
#include "rendering/resources/Shader.h"
#include "rendering/resources/Mesh.h"
#include "rendering/resources/Material.h"
#include "rendering/resources/MeshLoader.h"
#include <filesystem>
#include "rendering/Camera.h"
#include "rendering/Frustum.h"
#include "rendering/GBuffer.h"
#include "rendering/ShadowMap.h"
#include "rendering/resources/Shader.h"
#include "rendering/resources/Cubemap.h"
#include "rendering/SkyboxRenderer.h"
#include "rendering/RenderPipeline.h"
#include "rendering/IBL.h"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

using namespace OpenEye::Core;
using namespace OpenEye::Rendering;
using namespace OpenEye::ECS;

int main() {
    // Initialize logging
    Log::Initialize("editor.log");
    Log::SetLevel(LogLevel::Info);
    OE_INFO("=== OpenEye Editor Starting ===");
    
    // Initialize shader file watcher (syncs shaders from root to executable directory)
    // Copy from: assets/shaders (project root)
    // Copy to: assets/shaders (relative to executable, i.e., build/bin/Debug/assets/shaders)
    ShaderFileWatcher::Initialize("assets/shaders", "assets/shaders");
    
    // Initialize window
    Window window;
    WindowConfig config;
    config.title = "OpenEye Editor";
    config.width = 1920;
    config.height = 1080;
    config.vsync = true;
    config.debugContext = true;
    
    if (!window.Initialize(config)) {
        OE_FATAL("Failed to initialize window");
        Log::Shutdown();
        return -1;
    }
    
    // Initialize ImGui (docking + viewports)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    ImGui_ImplGlfw_InitForOpenGL(window.GetNativeHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 450");
    
    // Initialize time
    Time::Initialize();
    
    OE_INFO("Creating ECS Scene...");
    Scene scene;
    
    OE_INFO("Loading shaders and creating resources...");
    auto shader = std::make_shared<Shader>();
    if (!shader->LoadFromFile("assets/shaders/passes/forward/basic.vert", "assets/shaders/passes/forward/basic.frag")) {
        OE_ERROR("Failed to load shader");
        Log::Shutdown();
        return -1;
    }
    
    auto mesh = MeshLoader::CreateCube();
    auto material = std::make_shared<Material>(shader);
    material->SetAlbedo(glm::vec3(0.8f, 0.3f, 0.3f));
    material->SetMetallic(0.0f);
    material->SetRoughness(0.5f);
    
    OE_INFO("Creating entities...");
    // Create rotating cube entity
    Entity cubeEntity = scene.CreateEntity("RotatingCube");
    auto& initialCubeTransform = scene.GetRegistry().get<Transform>(cubeEntity);
    initialCubeTransform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    
    scene.GetRegistry().emplace<Renderable>(cubeEntity, mesh, material);
    scene.GetRegistry().emplace<AABB>(cubeEntity); // AABB for culling (future)
    
    // Create a few more test cubes
    for (int i = 0; i < 3; i++) {
        Entity testCube = scene.CreateEntity("TestCube_" + std::to_string(i));
        auto& transform = scene.GetRegistry().get<Transform>(testCube);
        transform.position = glm::vec3((i - 1) * 2.0f, 0.0f, -2.0f);
        
        auto testMaterial = std::make_shared<Material>(shader);
        float hue = i / 3.0f;
        testMaterial->SetAlbedo(glm::vec3(
            0.5f + 0.5f * cos(hue * 6.28f),
            0.5f + 0.5f * cos((hue + 0.33f) * 6.28f),
            0.5f + 0.5f * cos((hue + 0.66f) * 6.28f)
        ));
        
        scene.GetRegistry().emplace<Renderable>(testCube, mesh, testMaterial);
        scene.GetRegistry().emplace<AABB>(testCube);
    }
    
    // Add some test point lights for multiple light testing
    Entity pointLight1 = scene.CreateEntity("PointLight1");
    auto& light1Transform = scene.GetRegistry().get<Transform>(pointLight1);
    light1Transform.position = glm::vec3(2.0f, 2.0f, 0.0f);
    Light light1;
    light1.type = Light::Type::Point;
    light1.color = glm::vec3(1.0f, 0.8f, 0.6f); // Warm yellow
    light1.intensity = 5.0f;
    light1.range = 10.0f;
    scene.GetRegistry().emplace<Light>(pointLight1, light1);
    
    Entity pointLight2 = scene.CreateEntity("PointLight2");
    auto& light2Transform = scene.GetRegistry().get<Transform>(pointLight2);
    light2Transform.position = glm::vec3(-2.0f, 2.0f, -2.0f);
    Light light2;
    light2.type = Light::Type::Point;
    light2.color = glm::vec3(0.6f, 0.8f, 1.0f); // Cool blue
    light2.intensity = 4.0f;
    light2.range = 8.0f;
    scene.GetRegistry().emplace<Light>(pointLight2, light2);
    
    // Load glTF model with textures
    auto resolveAssetPath = [](const std::string& relative) -> std::string {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path current = fs::current_path(ec);
        fs::path rel(relative);
        for (fs::path p = current; !p.empty(); p = p.parent_path()) {
            fs::path candidate = p / rel;
            if (fs::exists(candidate, ec)) return candidate.string();
            if (p == p.parent_path()) break;
        }
        return relative;
    };
    
    std::string modelPath = resolveAssetPath("assets/models/dess_holiday_hb_juan_deltarune/scene.gltf");
    if (std::filesystem::exists(modelPath)) {
        OE_INFO("Loading model: " + modelPath);
        auto loadedMeshes = MeshLoader::LoadSceneFromFile(modelPath);
        
        if (!loadedMeshes.empty()) {
            OE_INFO("Loaded " + std::to_string(loadedMeshes.size()) + " meshes from model");
            
            for (size_t i = 0; i < loadedMeshes.size(); ++i) {
                const auto& loadedMesh = loadedMeshes[i];
                if (!loadedMesh->mesh || loadedMeshes[i]->materials.empty()) continue;
                
                // Check if mesh has multiple submeshes with different materials
                size_t submeshCount = loadedMesh->mesh->GetSubmeshCount();
                size_t materialCount = loadedMesh->materials.size();
                
                OE_INFO("Mesh '" + loadedMesh->name + "': " + 
                       std::to_string(submeshCount) + " submeshes, " + 
                       std::to_string(materialCount) + " materials");
                
                // For now, create one entity per mesh (with first material)
                // TODO: If mesh has multiple submeshes with different materials, 
                // we should create separate entities or handle submesh rendering per-material
                Entity modelEntity = scene.CreateEntity(loadedMesh->name.empty() ? "ModelMesh_" + std::to_string(i) : loadedMesh->name);
                auto& modelTransform = scene.GetRegistry().get<Transform>(modelEntity);
                
                // Position model - center it and scale down if needed
                modelTransform.position = glm::vec3(0.0f, 0.0f, -5.0f); // Place in front of camera
                modelTransform.scale = glm::vec3(1.0f); // Adjust scale if model is too large
                
                // Use the material from loaded mesh (which should have textures)
                auto modelMaterial = loadedMesh->materials[0];
                if (!modelMaterial) {
                    modelMaterial = std::make_shared<Material>(shader);
                }
                // Ensure material has the correct shader for deferred rendering
                if (modelMaterial && !modelMaterial->GetShader()) {
                    // Load gbuffer shader for deferred rendering
                    auto gbufferShader = std::make_shared<Shader>();
                    gbufferShader->LoadFromFile("assets/shaders/passes/deferred/gbuffer.vert", 
                                                "assets/shaders/passes/deferred/gbuffer.frag");
                    modelMaterial->SetShader(gbufferShader);
                }
                
                scene.GetRegistry().emplace<Renderable>(modelEntity, loadedMesh->mesh, modelMaterial);
                scene.GetRegistry().emplace<AABB>(modelEntity);
                
                // Log loaded textures for debugging
                OE_INFO("Model entity '" + loadedMesh->name + "' created");
                if (modelMaterial->GetTexture("u_AlbedoTexture")) {
                    OE_INFO("  - Albedo texture loaded");
                }
                if (modelMaterial->GetTexture("u_NormalTexture")) {
                    OE_INFO("  - Normal texture loaded");
                }
                if (modelMaterial->GetTexture("u_MetallicRoughnessTexture")) {
                    OE_INFO("  - MetallicRoughness texture loaded");
                }
                if (modelMaterial->GetTexture("u_MetallicTexture")) {
                    OE_INFO("  - Metallic texture loaded");
                }
                if (modelMaterial->GetTexture("u_RoughnessTexture")) {
                    OE_INFO("  - Roughness texture loaded");
                }
                if (modelMaterial->GetTexture("u_AOTexture")) {
                    OE_INFO("  - AO texture loaded");
                }
                if (modelMaterial->GetTexture("u_EmissiveTexture")) {
                    OE_INFO("  - Emissive texture loaded");
                }
            }
        } else {
            OE_WARN("Failed to load model from: " + modelPath);
        }
    } else {
        OE_WARN("Model file not found: " + modelPath);
    }
    
    // Count entities with Transform component (all entities should have this)
    size_t entityCount = scene.GetRegistry().view<Transform>().size();
    OE_INFO_F("Created %zu entities in scene", entityCount);
    
    // Camera setup (using standalone camera for now, will integrate with ECS later)
    Camera camera;
    camera.SetPosition(glm::vec3(0.0f, 0.0f, 5.0f));
    camera.SetPerspective(45.0f, window.GetAspectRatio(), 0.1f, 100.0f);
    camera.SetMovementSpeed(5.0f);
    camera.SetMouseSensitivity(0.1f);
    
    bool rightMouseButtonDown = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;
    
    // Setup input callbacks (after camera is created)
    window.SetKeyCallback([&](int key, int scancode, int action, int mods) {
        Input::OnKeyEvent(key, scancode, action, mods);
        
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            window.SetShouldClose(true);
        }
        
        // Camera keyboard input
        if (action == GLFW_PRESS || action == GLFW_RELEASE) {
            camera.ProcessKeyboard(key, action == GLFW_PRESS);
        }
    });
    
    window.SetMouseButtonCallback([&](int button, int action, int mods) {
        Input::OnMouseButtonEvent(button, action, mods);
        
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            rightMouseButtonDown = (action == GLFW_PRESS);
            if (rightMouseButtonDown) {
                window.SetMouseLocked(true);
                firstMouse = true;
            } else {
                window.SetMouseLocked(false);
            }
        }
    });
    
    window.SetMouseMoveCallback([&](double x, double y) {
        Input::OnMouseMoveEvent(x, y);
        
        if (rightMouseButtonDown) {
            if (firstMouse) {
                lastMouseX = x;
                lastMouseY = y;
                firstMouse = false;
            }
            
            double deltaX = x - lastMouseX;
            double deltaY = y - lastMouseY;
            
            camera.ProcessMouseMovement(static_cast<float>(deltaX), static_cast<float>(deltaY), true);
            
            lastMouseX = x;
            lastMouseY = y;
        }
    });
    
    window.SetScrollCallback([&](double xoffset, double yoffset) {
        Input::OnScrollEvent(xoffset, yoffset);
        camera.ProcessMouseScroll(static_cast<float>(yoffset));
    });
    
    OE_INFO("Editor initialized successfully");
    OE_INFO("Controls: WASD (move), QE (up/down), Right-click drag (rotate), Scroll (zoom)");
    OE_INFO("Press ESC to exit");
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    
    // Debug and lighting settings (UI-controlled)
    static int debugOutputMode = 0; // 0=Lighting,1=Albedo,2=Normal,3=Position,4=PBR
    static glm::vec3 uiLightDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.3f));
    static glm::vec3 uiLightColor = glm::vec3(1.0f, 0.95f, 0.9f);
    static float uiLightIntensity = 3.0f;
    static glm::vec3 uiAmbient = glm::vec3(0.08f);
    static float uiExposure = 1.0f;
    // Render toggles
    static bool uiEnableCulling = true;
    static bool uiEnableLOD = true;
    static bool uiEnableInstancing = false;
    static int uiMinInstancesForBatch = 3;
    
    // GBuffer & ShadowMap (deferred path resources)
    static RenderPipeline renderPipeline;
    static bool pipelineInitialized = false;
    
    // Main loop
    while (!window.ShouldClose()) {
        Time::Update();
        Input::Update();
        
        window.PollEvents();
        
        // Update camera
        float deltaTime = Time::DeltaTime();
        camera.Update(deltaTime);
        
        // Update projection if window was resized
        camera.SetPerspective(camera.GetFOV(), window.GetAspectRatio(), 0.1f, 100.0f);
        
        // Update ECS systems
        TransformSystem::Update(scene);
        
        // Rotate the main cube
        float time = Time::TotalTime();
        auto& cubeTransform = scene.GetRegistry().get<Transform>(cubeEntity);
        cubeTransform.rotation = glm::angleAxis(time * 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        TransformSystem::MarkDirty(scene.GetRegistry(), cubeEntity);
        
        // Clear screen
        window.Clear(0.1f, 0.1f, 0.15f, 1.0f);
        
        // Update AABBs when transforms change
        // Note: Should be done when transforms change, but for now update every frame
        // In production, only update when transform is dirty
        CullingSystem::UpdateAABBs(scene);
        
        // Extract frustum for culling
        Frustum frustum;
        glm::mat4 viewProj = camera.GetProjectionMatrix() * camera.GetViewMatrix();
        frustum.ExtractFromMatrix(viewProj);
        
        // Toggle to compare: forward (basic) vs deferred
        static const bool useDeferred = true;  // Deferred pipeline enabled
        if (!useDeferred) {
            // Forward path: draw the main cube directly, bypassing culling/ECS
            glm::mat4 view = camera.GetViewMatrix();
            glm::mat4 proj = camera.GetProjectionMatrix();
            
            // Use the material/shader created for the cube
            material->Bind();
            auto forwardShader = material->GetShader();
            if (forwardShader) {
                // Model matrix from ECS transform
                glm::mat4 model = cubeTransform.GetWorldMatrix(scene.GetRegistry());
                forwardShader->SetMat4("u_Model", model);
                forwardShader->SetMat4("u_View", view);
                forwardShader->SetMat4("u_Projection", proj);
                forwardShader->SetVec3("u_ViewPos", camera.GetPosition());
                forwardShader->SetVec3("u_LightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
                forwardShader->SetVec3("u_LightColor", glm::vec3(1.0f));
            }
            
            mesh->Bind();
            mesh->Draw();
            mesh->Unbind();
            
            if (forwardShader) {
                forwardShader->Unbind();
            }
            
            // Optionally render others via ECS forward path (without culling)
            // RenderSystem::Update(scene, camera, view, proj);
        } else {
            // Deferred rendering pipeline (centralized)
            if (!pipelineInitialized) {
                renderPipeline.Initialize(static_cast<uint32_t>(window.GetWidth()), static_cast<uint32_t>(window.GetHeight()));
                // Load skybox through pipeline cubemap
                auto resolveSkyboxPath = [](const std::string& relative) -> std::string {
                    namespace fs = std::filesystem;
                    std::error_code ec;
                    fs::path current = fs::current_path(ec);
                    fs::path rel(relative);
                    for (fs::path p = current; !p.empty(); p = p.parent_path()) {
                        fs::path candidate = p / rel;
                        if (fs::exists(candidate, ec)) return candidate.string();
                        if (p == p.parent_path()) break;
                    }
                    return relative;
                };
                std::vector<std::string> faces = {
                    resolveSkyboxPath("assets/Clear Ocean/vz_clear_ocean_right.png"),
                    resolveSkyboxPath("assets/Clear Ocean/vz_clear_ocean_left.png"),
                    resolveSkyboxPath("assets/Clear Ocean/vz_clear_ocean_up.png"),
                    resolveSkyboxPath("assets/Clear Ocean/vz_clear_ocean_down.png"),
                    resolveSkyboxPath("assets/Clear Ocean/vz_clear_ocean_front.png"),
                    resolveSkyboxPath("assets/Clear Ocean/vz_clear_ocean_back.png")
                };
                if (auto* skybox = renderPipeline.GetSkyboxCubemap()) {
                    if (skybox->LoadFromFiles(faces, true)) {
                        OE_INFO("Skybox loaded: Clear Ocean");
                        // Initialize IBL from skybox
                        if (auto* ibl = renderPipeline.GetIBL()) {
                            GLuint skyboxTexID = skybox->GetID();
                            if (!ibl->Initialize(skyboxTexID, 32, 512)) {
                                OE_WARN("Failed to initialize IBL from skybox");
                            } else {
                                OE_INFO("IBL initialized from skybox");
                            }
                        }
                    } else {
                        OE_WARN("Failed to load skybox.");
                    }
                }
                pipelineInitialized = true;
            }
            renderPipeline.Resize(static_cast<uint32_t>(window.GetWidth()), static_cast<uint32_t>(window.GetHeight()));

            LightingParams params;
            params.debugMode = debugOutputMode;
            params.lightDir = uiLightDir;
            params.lightColor = uiLightColor;
            params.lightIntensity = uiLightIntensity;
            params.ambient = uiAmbient;
            params.exposure = uiExposure;
            // Build render options
            RenderOptions opts;
            opts.enableCulling = uiEnableCulling;
            opts.enableLOD = uiEnableLOD;
            opts.enableInstancing = uiEnableInstancing;
            opts.minInstancesForBatch = uiMinInstancesForBatch;
            renderPipeline.RenderFrame(scene, camera, params, opts);
        }
        
        // --- ImGui UI ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Tools dockspace (floating window) - main scene stays untouched beneath
        static bool openToolsDock = true;
        ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Tools", &openToolsDock, ImGuiWindowFlags_MenuBar)) {
            ImGuiID tools_dockspace = ImGui::GetID("ToolsDockSpace");
            ImGui::DockSpace(tools_dockspace, ImVec2(0.0f, 0.0f));
            
            // Renderer Debug docks here on first open
            ImGui::SetNextWindowDockID(tools_dockspace, ImGuiCond_FirstUseEver);
        }
        ImGui::End();
        
        // Renderer Debug panel (docked into Tools window)
        ImGui::Begin("Renderer Debug");
        const char* modes[] = {"Lighting", "Albedo", "Normal", "Position", "PBR"};
        ImGui::Combo("Output", &debugOutputMode, modes, IM_ARRAYSIZE(modes));
        ImGui::Separator();
        ImGui::Text("Render Options");
        ImGui::Checkbox("Frustum Culling", &uiEnableCulling);
        ImGui::Checkbox("LOD", &uiEnableLOD);
        ImGui::Checkbox("Instancing (deferred)", &uiEnableInstancing);
        ImGui::DragInt("Min Instances for Batch", &uiMinInstancesForBatch, 1, 1, 1024);
        ImGui::Separator();
        ImGui::Text("Lighting");
        ImGui::DragFloat3("Dir", &uiLightDir.x, 0.01f, -1.0f, 1.0f);
        if (glm::length(uiLightDir) < 0.001f) uiLightDir = glm::vec3(0.0f,1.0f,0.0f);
        ImGui::ColorEdit3("Color", &uiLightColor.x);
        ImGui::DragFloat("Intensity", &uiLightIntensity, 0.05f, 0.0f, 50.0f);
        ImGui::DragFloat3("Ambient", &uiAmbient.x, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Exposure", &uiExposure, 0.05f, 0.01f, 5.0f);
        if (useDeferred && pipelineInitialized && renderPipeline.GetGBuffer()) {
            ImGui::Separator();
            ImGui::Text("G-Buffer");
            ImGui::Text("Position");
            ImGui::Image((ImTextureID)(intptr_t)renderPipeline.GetGBuffer()->GetPositionTexture(), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
            ImGui::SameLine(); ImGui::Text("Normal");
            ImGui::Image((ImTextureID)(intptr_t)renderPipeline.GetGBuffer()->GetNormalTexture(), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
            ImGui::SameLine(); ImGui::Text("Albedo");
            ImGui::Image((ImTextureID)(intptr_t)renderPipeline.GetGBuffer()->GetAlbedoTexture(), ImVec2(256, 256), ImVec2(0,1), ImVec2(1,0));
        }
        ImGui::End();

        // FrameGraph panel (execution order and timings)
        ImGui::Begin("FrameGraph");
        if (useDeferred && pipelineInitialized) {
            const auto& passes = renderPipeline.GetFrameGraphPasses();
            ImGui::Columns(3, nullptr, false);
            ImGui::Text("Pass"); ImGui::NextColumn();
            ImGui::Text("GPU ms"); ImGui::NextColumn();
            ImGui::Text("Status"); ImGui::NextColumn();
            ImGui::Separator();
            for (const auto& p : passes) {
                ImGui::TextUnformatted(p.name.c_str()); ImGui::NextColumn();
                ImGui::Text("%.3f", p.gpuMs); ImGui::NextColumn();
                ImGui::TextUnformatted(p.culled ? "culled" : "executed"); ImGui::NextColumn();
            }
            ImGui::Columns(1);
        } else {
            ImGui::TextUnformatted("Not available (deferred disabled or pipeline not initialized)");
        }
        ImGui::End();
        
        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
        
        // Swap buffers
        window.SwapBuffers();
        
        // Log FPS every second
        static float lastFpsLog = 0.0f;
        if (Time::TotalTime() - lastFpsLog >= 1.0f) {
            OE_INFO_F("FPS: %.1f | Frame: %llu | DeltaTime: %.4f", 
                      Time::FPS(), Time::FrameCount(), Time::DeltaTime());
            lastFpsLog = Time::TotalTime();
        }
    }
    
    OE_INFO("=== OpenEye Editor Shutting Down ===");
    // Shutdown ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    window.Shutdown();
    Log::Shutdown();
    
    return 0;
}

