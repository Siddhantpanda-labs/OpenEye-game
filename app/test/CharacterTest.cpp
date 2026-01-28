#include "CharacterTest.h"
#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "api/assets/loaders/ModelLoader.h"
#include "api/animation/All.h"
#include "eyelib/math/Types.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>

namespace eyelib {
namespace test {

CharacterTest::CharacterTest() = default;

CharacterTest::~CharacterTest() {
    Shutdown();
}

bool CharacterTest::Initialize(int windowWidth, int windowHeight) {
    m_windowWidth = windowWidth;
    m_windowHeight = windowHeight;

    // Initialize window
    using namespace api::platform;
    m_window = std::make_shared<api::Window>();
    WindowConfig cfg;
    cfg.title = "OpenEye - Third-Person Character Test";
    cfg.width = windowWidth;
    cfg.height = windowHeight;
    cfg.debugContext = false;
    cfg.resizable = true;

    if (!m_window->Initialize(cfg)) {
        api::core::Log::Error("Failed to initialize window");
        return false;
    }

    // Initialize renderer
    m_renderer = std::make_shared<api::Renderer>();
    if (!m_renderer->Initialize()) {
        api::core::Log::Error("Failed to initialize renderer");
        m_window->Shutdown();
        return false;
    }

    // Initialize render pipeline
    m_pipeline = std::make_shared<api::RenderPipeline>();
    if (!m_pipeline->Initialize(windowWidth, windowHeight)) {
        api::core::Log::Error("Failed to initialize render pipeline");
        m_renderer->Shutdown();
        m_window->Shutdown();
        return false;
    }
    m_pipeline->SetMode(api::RenderPipeline::Mode::Forward);

    // Setup free-flying camera
    m_camera.SetMoveSpeed(5.0f);
    m_camera.SetSensitivity(0.15f);

    // Setup character
    // DEBUG: Lock character position to origin for in-place animation testing
    m_character.SetPosition(math::Vec3(0.0f, 0.0f, 0.0f));
    m_character.SetWalkSpeed(3.0f);
    m_character.SetRunSpeed(6.0f);
    m_character.SetRotationSpeed(360.0f);

    // Setup scene
    SetupScene();

    api::core::Log::Info("Character test initialized successfully");
    api::core::Log::Info("Controls: WASD = Move, Mouse = Look around, Scroll = Zoom");

    return true;
}

void CharacterTest::SetupScene() {
    using namespace api::render;
    using namespace math;

    // Setup camera projection
    m_scene.camera.projection = glm::perspective(
        glm::radians(60.0f),
        static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight),
        0.1f, 1000.0f);

    // Setup lighting
    m_scene.light.direction = Vec3(-0.4f, -1.0f, -0.2f);
    m_scene.light.color = Vec3(1.0f, 1.0f, 1.0f);
    m_scene.light.intensity = 2.0f;
    m_scene.light.ambient = Vec3(0.1f);
    m_scene.light.exposure = 1.0f;

    // Load skybox (optional)
    auto skybox = std::make_shared<api::TextureCube>();
    std::vector<std::string> skyFaces = {
        "assets/Clear Ocean/vz_clear_ocean_right.png",
        "assets/Clear Ocean/vz_clear_ocean_left.png",
        "assets/Clear Ocean/vz_clear_ocean_up.png",
        "assets/Clear Ocean/vz_clear_ocean_down.png",
        "assets/Clear Ocean/vz_clear_ocean_front.png",
        "assets/Clear Ocean/vz_clear_ocean_back.png"
    };
    if (skybox->LoadFromFiles(skyFaces, true, true)) {
        skybox->SetFilter(api::TextureFilter::LinearMipmapLinear, api::TextureFilter::Linear);
        skybox->SetWrap(api::TextureWrap::ClampToEdge, api::TextureWrap::ClampToEdge, api::TextureWrap::ClampToEdge);
        m_scene.skybox = skybox;
    }
}

bool CharacterTest::LoadCharacterModel(const std::string& modelPath) {
    api::core::Log::InfoF("Loading character model: %s", modelPath.c_str());

    // Load model
    m_characterModel = api::ModelLoader::LoadModel(modelPath);
    if (!m_characterModel) {
        api::core::Log::ErrorF("Failed to load model: %s", modelPath.c_str());
        return false;
    }

    // Get meshes and materials
    const auto& meshes = m_characterModel->GetMeshes();
    const auto& materials = m_characterModel->GetMaterials();

    if (meshes.empty()) {
        api::core::Log::Error("Model has no meshes");
        return false;
    }

    // Use first mesh (assuming single mesh character)
    m_characterMesh = meshes[0];
    
    // Get skeleton
    m_skeleton = m_characterModel->GetSkeleton();
    if (!m_skeleton) {
        api::core::Log::Warn("Model has no skeleton - animations will not work");
    }

    // Create or use material
    if (!materials.empty()) {
        m_characterMaterial = materials[0];
    } else {
        // Create default material
        m_characterMaterial = std::make_shared<api::Material>();
        auto shader = std::make_shared<api::Shader>();
        if (shader->LoadFromFile("assets/shaders/passes/forward/skinned.vert",
                                 "assets/shaders/passes/forward/skinned.frag")) {
            m_characterMaterial->SetShader(shader);
        } else {
            api::core::Log::Warn("Failed to load skinned shader, using basic shader");
            shader = std::make_shared<api::Shader>();
            if (shader->LoadFromFile("assets/shaders/passes/forward/basic.vert",
                                     "assets/shaders/passes/forward/basic.frag")) {
                m_characterMaterial->SetShader(shader);
            }
        }
    }

    // Setup animator if skeleton exists
    if (m_skeleton) {
        m_animator = std::make_shared<api::Animator>();
        m_animator->SetSkeleton(m_skeleton);
        
        // DEBUG: Enable root motion extraction but disable application for in-place testing
        // Root motion extraction neutralizes the root bone (makes it identity visually)
        // But we don't apply the extracted motion to character transform (stays in place)
        // Empty string will auto-detect root bone (bone with parentIndex == -1)
        m_animator->SetRootMotionBone(""); // Auto-detect root bone
        m_animator->SetRootMotionEnabled(true); // Extract root motion to neutralize root bone
        
        // DEBUG: Set motion authority to AnimationDriven to disable input-based movement
        // This ensures only root motion (if applied) controls movement, not WASD input
        m_animator->SetMotionAuthority(api::Animator::MotionAuthority::AnimationDriven);
        
        // DEBUG: Force run animation for in-place debugging
        // Use run clip if available, otherwise use default clip from model
        auto defaultClip = m_characterModel->GetClip();
        if (defaultClip) {
            m_idleClip = defaultClip;
            // If run clip not loaded yet, use default clip as fallback
            if (!m_runClip) {
                m_runClip = defaultClip;
            }
            // DEBUG: Set run clip directly for in-place testing
            if (m_runClip) {
                m_animator->SetClip(m_runClip);
                api::core::Log::Info("DEBUG: Run animation set to play in place (root motion disabled)");
            } else {
                m_animator->SetClip(defaultClip);
                api::core::Log::Info("DEBUG: Default animation set to play in place (root motion disabled)");
            }
        }
        
        // DEBUG: Force character to run state
        m_character.SetRunning(true);
    }

    api::core::Log::InfoF("Character model loaded: %zu meshes, skeleton: %s",
                         meshes.size(), m_skeleton ? "yes" : "no");

    return true;
}

void CharacterTest::LoadAnimations(const std::string& idlePath,
                                   const std::string& walkPath,
                                   const std::string& runPath) {
    if (!m_skeleton) {
        api::core::Log::Warn("Cannot load animations: no skeleton");
        return;
    }

    // Load idle animation
    if (!idlePath.empty()) {
        auto idleModel = api::ModelLoader::LoadModel(idlePath);
        if (idleModel && idleModel->GetClip()) {
            m_idleClip = idleModel->GetClip();
            if (m_animator) {
                m_animator->SetClip(m_idleClip);
            }
            api::core::Log::InfoF("Loaded idle animation: %s", idlePath.c_str());
        } else {
            api::core::Log::WarnF("Failed to load idle animation: %s", idlePath.c_str());
        }
    }

    // Load walk animation
    if (!walkPath.empty()) {
        auto walkModel = api::ModelLoader::LoadModel(walkPath);
        if (walkModel && walkModel->GetClip()) {
            m_walkClip = walkModel->GetClip();
            api::core::Log::InfoF("Loaded walk animation: %s", walkPath.c_str());
        } else {
            api::core::Log::WarnF("Failed to load walk animation: %s", walkPath.c_str());
        }
    }

    // Load run animation
    if (!runPath.empty()) {
        auto runModel = api::ModelLoader::LoadModel(runPath);
        if (runModel && runModel->GetClip()) {
            m_runClip = runModel->GetClip();
            // DEBUG: If animator exists, set run clip immediately for in-place testing
            if (m_animator) {
                m_animator->SetClip(m_runClip);
                api::core::Log::InfoF("DEBUG: Run animation set to play in place: %s", runPath.c_str());
            }
            api::core::Log::InfoF("Loaded run animation: %s", runPath.c_str());
        } else {
            api::core::Log::WarnF("Failed to load run animation: %s", runPath.c_str());
        }
    }
}

void CharacterTest::ProcessInput(float deltaTime) {
    using namespace api::platform;

    const int KEY_W = 87;
    const int KEY_A = 65;
    const int KEY_S = 83;
    const int KEY_D = 68;
    const int KEY_Q = 81;
    const int KEY_E = 69;
    const int KEY_SPACE = 32;

    // Camera movement (WASD + QE)
    math::Vec3 cameraMoveInput(0.0f, 0.0f, 0.0f);
    if (Input::IsKeyPressed(KEY_W)) cameraMoveInput.y += 1.0f;
    if (Input::IsKeyPressed(KEY_S)) cameraMoveInput.y -= 1.0f;
    if (Input::IsKeyPressed(KEY_D)) cameraMoveInput.x += 1.0f;
    if (Input::IsKeyPressed(KEY_A)) cameraMoveInput.x -= 1.0f;
    if (Input::IsKeyPressed(KEY_E)) cameraMoveInput.z += 1.0f;  // Up
    if (Input::IsKeyPressed(KEY_Q)) cameraMoveInput.z -= 1.0f;  // Down

    // Normalize camera movement input if diagonal
    float cameraMoveLength = glm::length(math::Vec2(cameraMoveInput.x, cameraMoveInput.y));
    if (cameraMoveLength > 1.0f) {
        cameraMoveInput.x /= cameraMoveLength;
        cameraMoveInput.y /= cameraMoveLength;
    }

    // Update camera movement
    m_camera.UpdateMovement(cameraMoveInput, deltaTime);

    // Character movement (WASD relative to camera)
    math::Vec2 characterMoveInput(0.0f, 0.0f);
    if (Input::IsKeyPressed(KEY_W)) characterMoveInput.y += 1.0f;
    if (Input::IsKeyPressed(KEY_S)) characterMoveInput.y -= 1.0f;
    if (Input::IsKeyPressed(KEY_D)) characterMoveInput.x += 1.0f;
    if (Input::IsKeyPressed(KEY_A)) characterMoveInput.x -= 1.0f;

    // Normalize character movement input if diagonal
    float characterMoveLength = glm::length(characterMoveInput);
    if (characterMoveLength > 1.0f) {
        characterMoveInput = characterMoveInput / characterMoveLength;
    }

    // Set running state (space bar)
    bool isRunning = Input::IsKeyPressed(KEY_SPACE);
    m_character.SetRunning(isRunning);

    // Update character movement relative to camera
    // Get motion authority from animator
    api::Animator::MotionAuthority motionAuthority = api::Animator::MotionAuthority::ControllerDriven;
    if (m_animator) {
        motionAuthority = m_animator->GetMotionAuthority();
    }
    
    math::Vec3 cameraForward = m_camera.GetForward();
    m_character.UpdateMovement(characterMoveInput, cameraForward, deltaTime, motionAuthority);

    // Update camera with mouse input (right click to look around)
    m_mouseRightPressed = Input::IsMouseButtonPressed(1); // Right mouse button
    
    if (m_mouseRightPressed) {
        auto mouseDelta = Input::MouseDelta();
        m_camera.UpdateInput(math::Vec2(mouseDelta.x, mouseDelta.y));
    }

    // Update input state
    Input::Update();
}

void CharacterTest::UpdateCamera(float deltaTime) {
    // Update camera (position is updated via ProcessInput)
    m_camera.Update(deltaTime);

    // Update scene camera
    m_scene.camera.view = m_camera.GetViewMatrix();
    m_scene.camera.viewPos = m_camera.GetPosition();
}

void CharacterTest::UpdateCharacter(float deltaTime) {
    // DEBUG: Lock character position to origin to prevent any movement
    m_character.SetPosition(math::Vec3(0.0f, 0.0f, 0.0f));
    
    // Update character animations
    if (m_animator) {
        // DEBUG: Skip normal animation state updates - run clip is set in initialization
        // m_character.UpdateAnimations(m_animator, m_idleClip, m_walkClip, m_runClip);
        
        m_animator->Update(deltaTime);
        
        // DEBUG: Disable root motion application - animation plays in place
        // This helps debug snapping issues by isolating animation playback from root motion
        // The animation should loop smoothly without any position snapping
        // Root motion is still extracted in library, but not applied to character transform
        // TODO: Re-enable after debugging
        /*
        // Apply root motion delta to character (respects motion authority)
        auto rootMotionDelta = m_animator->GetRootMotionDelta();
        if (rootMotionDelta.hasMotion) {
            auto motionAuthority = m_animator->GetMotionAuthority();
            m_character.ApplyRootMotion(rootMotionDelta, motionAuthority);
        }
        */
    }
}

void CharacterTest::Update(float deltaTime) {
    if (!m_window) return;

    m_window->PollEvents();

    // Handle window resize
    const int w = m_window->Width();
    const int h = m_window->Height();
    if (w != m_windowWidth || h != m_windowHeight) {
        m_windowWidth = w;
        m_windowHeight = h;
        m_pipeline->Resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        m_scene.camera.projection = glm::perspective(
            glm::radians(60.0f),
            static_cast<float>(w) / static_cast<float>(h),
            0.1f, 1000.0f);
    }

    // Process input
    ProcessInput(deltaTime);

    // Update camera
    UpdateCamera(deltaTime);

    // Update character
    UpdateCharacter(deltaTime);

    // Update scene drawables
    m_scene.opaque.clear();
    
    if (m_characterMesh && m_characterMaterial) {
        api::Drawable drawable;
        drawable.mesh = m_characterMesh;
        drawable.material = m_characterMaterial;
        drawable.transform = m_character.GetTransform();
        
        // Set bone matrices for skinned rendering
        if (m_animator && !m_animator->GetBoneMatrices().empty()) {
            const auto& boneMatrices = m_animator->GetBoneMatrices();
            // Set bone matrices on material (shader expects "u_Bones" uniform)
            m_characterMaterial->SetMat4Array("u_Bones", boneMatrices);
        }
        
        m_scene.opaque.push_back(drawable);
    }
}

void CharacterTest::Render() {
    if (!m_renderer || !m_pipeline) return;

    m_renderer->Clear(0.1f, 0.1f, 0.15f, 1.0f);
    m_pipeline->Render(m_scene);
    m_pipeline->BlitToScreen();
    m_window->SwapBuffers();
}

bool CharacterTest::ShouldClose() const {
    return m_window ? m_window->ShouldClose() : true;
}

void CharacterTest::Shutdown() {
    if (m_pipeline) {
        m_pipeline->Shutdown();
        m_pipeline.reset();
    }
    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }
    if (m_window) {
        m_window->Shutdown();
        m_window.reset();
    }
}

} // namespace test
} // namespace eyelib
