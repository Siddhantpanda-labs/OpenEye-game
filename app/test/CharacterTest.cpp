#include "CharacterTest.h"
#include "api/core/All.h"
#include "api/platform/All.h"
#include "api/render/All.h"
#include "api/debug/DebugDraw.h"
#include "api/debug/Presets.h"
#include "api/assets/loaders/ModelLoader.h"
#include "api/animation/All.h"
#include "eyelib/animation/StateMachineConditions.h"
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

    // Enable debug draw so skeleton/debug lines are visible (DebugDraw::AddLine, etc.)
    api::debug::Presets::SetDebugDrawEnabled(true);

    // Setup free-flying camera
    m_camera.SetMoveSpeed(5.0f);
    m_camera.SetSensitivity(0.15f);

    // Setup character
    m_character.SetPosition(math::Vec3(0.0f, 0.0f, 0.0f));
    m_character.SetWalkSpeed(3.0f);
    m_character.SetRunSpeed(6.0f);
    m_character.SetRotationSpeed(360.0f);

    // Setup scene
    SetupScene();

    api::core::Log::Info("Character test initialized successfully");
    api::core::Log::Info("Controls: Mouse = Look; WASD = walk (smooth turn), +Space = run; release = idle");

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

        // Rotation pivot: use AABB center so the mesh rotates around its visual center
        if (m_characterMesh && m_characterMesh->IsValid()) {
            const math::AABB aabb = m_characterMesh->GetAABB();
            if (aabb.IsValid()) {
                m_character.SetPivotOffset(aabb.GetCenter());
            }
        }

        // Root motion: extracted from walk/run clips and applied when W/S pressed
        m_animator->SetRootMotionBone("");
        m_animator->SetRootMotionEnabled(true);

        // Set walk animation to play in place
        if (m_walkClip) {
            m_animator->SetClip(m_walkClip);
        } else if (m_idleClip) {
            m_animator->SetClip(m_idleClip);
        } else if (m_characterModel->GetClip()) {
            m_animator->SetClip(m_characterModel->GetClip());
        }
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
            // Set walk animation immediately if animator exists
            if (m_animator) {
                m_animator->SetClip(m_walkClip);
            }
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
                api::core::Log::InfoF("Loaded run animation: %s", runPath.c_str());
            } else {
                api::core::Log::WarnF("Failed to load run animation: %s", runPath.c_str());
            }
        }

    // Idle / Walk / Run state machine: start in Idle; W = walk, W+Space = run, release W = idle
    if (m_animator && m_idleClip && m_walkClip && m_runClip) {
        m_stateMachine = std::make_shared<api::StateMachine>();
        const int idleState = m_stateMachine->AddState("Idle", m_idleClip, 1.0f, true);
        const int walkState = m_stateMachine->AddState("Walk", m_walkClip, 1.0f, true);
        const int runState = m_stateMachine->AddState("Run", m_runClip, 1.0f, true);
        const float blendDuration = 0.25f;

        auto toIdle = std::vector<std::shared_ptr<eyelib::animation::Condition>>{
            std::make_shared<eyelib::animation::TriggerCondition>("toIdle") };
        auto toWalk = std::vector<std::shared_ptr<eyelib::animation::Condition>>{
            std::make_shared<eyelib::animation::TriggerCondition>("toWalk") };
        auto toRun = std::vector<std::shared_ptr<eyelib::animation::Condition>>{
            std::make_shared<eyelib::animation::TriggerCondition>("toRun") };

        m_stateMachine->AddTransition(idleState, walkState, toWalk, blendDuration);
        m_stateMachine->AddTransition(idleState, runState, toRun, blendDuration);
        m_stateMachine->AddTransition(walkState, idleState, toIdle, blendDuration);
        m_stateMachine->AddTransition(walkState, runState, toRun, blendDuration);
        m_stateMachine->AddTransition(runState, idleState, toIdle, blendDuration);
        m_stateMachine->AddTransition(runState, walkState, toWalk, blendDuration);

        m_stateMachine->SetInitialState(idleState);
        m_animator->SetStateMachine(m_stateMachine);
        api::core::Log::Info("Idle/Walk/Run: W = walk, W+Space = run, release W = idle");
    }
}

void CharacterTest::ProcessInput(float deltaTime) {
    using namespace api::platform;

    const int KEY_W = 87;
    const int KEY_A = 65;
    const int KEY_S = 83;
    const int KEY_D = 68;
    const int KEY_SPACE = 32;
    const int KEY_R = 82;

    bool w = Input::IsKeyPressed(KEY_W);
    bool a = Input::IsKeyPressed(KEY_A);
    bool s = Input::IsKeyPressed(KEY_S);
    bool d = Input::IsKeyPressed(KEY_D);
    bool space = Input::IsKeyPressed(KEY_SPACE);
    bool r = Input::IsKeyPressed(KEY_R);

    bool anyMove = w || a || s || d;

    // Idle / Walk / Run: WASD = walk, WASD+Space = run, release all = idle
    if (m_animator && m_stateMachine) {
        if (anyMove && space)
            m_animator->SetTrigger("toRun");
        else if (anyMove)
            m_animator->SetTrigger("toWalk");
        else
            m_animator->SetTrigger("toIdle");
    }

    m_forwardInput = anyMove ? 1 : 0;

    // Face character toward move direction (W/S/A/D relative to camera), smooth turn
    if (anyMove) {
        math::Vec3 camFwd = m_camera.GetForward();
        math::Vec3 camRight = m_camera.GetRight();
        camFwd.y = 0.0f;
        camRight.y = 0.0f;
        camFwd = glm::normalize(camFwd);
        camRight = glm::normalize(camRight);

        math::Vec3 moveDir(0.0f);
        if (w) moveDir += camFwd;
        if (s) moveDir -= camFwd;
        if (d) moveDir += camRight;
        if (a) moveDir -= camRight;
        moveDir.y = 0.0f;
        moveDir = glm::normalize(moveDir);
        m_moveDirWorld = moveDir;

        float targetYaw = std::atan2(moveDir.x, moveDir.z) * 180.0f / 3.14159265f;
        float currentYaw = m_character.GetRotation();
        float diff = targetYaw - currentYaw;
        while (diff > 180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        float maxRot = m_character.GetRotationSpeed() * deltaTime;
        float deltaRot = std::max(-maxRot, std::min(maxRot, diff));
        m_character.SetRotation(currentYaw + deltaRot);
    }
    if (!anyMove) {
        m_moveDirWorld = math::Vec3(0.0f, 0.0f, 0.0f);
    }

    // Debug: hold R to spin continuously around pivot
    if (r) {
        float spin = m_character.GetRotationSpeed() * deltaTime;
        m_character.SetRotation(m_character.GetRotation() + spin);
    }

    // Camera movement disabled - only look around is active
    // math::Vec3 cameraMoveInput(0.0f, 0.0f, 0.0f);
    // if (Input::IsKeyPressed(KEY_W)) cameraMoveInput.y += 1.0f;
    // if (Input::IsKeyPressed(KEY_S)) cameraMoveInput.y -= 1.0f;
    // if (Input::IsKeyPressed(KEY_D)) cameraMoveInput.x += 1.0f;
    // if (Input::IsKeyPressed(KEY_A)) cameraMoveInput.x -= 1.0f;
    // if (Input::IsKeyPressed(KEY_E)) cameraMoveInput.z += 1.0f;  // Up
    // if (Input::IsKeyPressed(KEY_Q)) cameraMoveInput.z -= 1.0f;  // Down
    // float cameraMoveLength = glm::length(math::Vec2(cameraMoveInput.x, cameraMoveInput.y));
    // if (cameraMoveLength > 1.0f) {
    //     cameraMoveInput.x /= cameraMoveLength;
    //     cameraMoveInput.y /= cameraMoveLength;
    // }
    // m_camera.UpdateMovement(cameraMoveInput, deltaTime);

    // Disabled: No input movement - just play animation in place
    // Character movement (WASD relative to camera)
    // math::Vec2 characterMoveInput(0.0f, 0.0f);
    // if (Input::IsKeyPressed(KEY_W)) characterMoveInput.y += 1.0f;
    // if (Input::IsKeyPressed(KEY_S)) characterMoveInput.y -= 1.0f;
    // if (Input::IsKeyPressed(KEY_D)) characterMoveInput.x += 1.0f;
    // if (Input::IsKeyPressed(KEY_A)) characterMoveInput.x -= 1.0f;

    // Normalize character movement input if diagonal
    // float characterMoveLength = glm::length(characterMoveInput);
    // if (characterMoveLength > 1.0f) {
    //     characterMoveInput = characterMoveInput / characterMoveLength;
    // }

    // Set running state (space bar)
    // bool isRunning = Input::IsKeyPressed(KEY_SPACE);
    // m_character.SetRunning(isRunning);

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
    if (!m_animator) return;

    m_animator->Update(deltaTime);

    // Apply root motion when in Walk or Run and W (forward) or S (backward) is pressed
    if (!m_stateMachine || m_forwardInput == 0) return;

    const int idleState = 0, walkState = 1, runState = 2;
    int currentState = m_stateMachine->GetInternal()->GetCurrentState();
    if (currentState != walkState && currentState != runState) return;

    auto delta = m_animator->GetRootMotionDelta();
    if (!delta.hasMotion) return;

    // Apply root motion in world space along input direction (decoupled from yaw)
    if (glm::length(m_moveDirWorld) > 0.0f) {
        api::Animator::RootMotionDelta worldDelta = delta;
        worldDelta.translation = m_moveDirWorld * delta.translation.z;
        worldDelta.translation.y += delta.translation.y;
        m_character.ApplyRootMotionWorldSpace(worldDelta, api::Animator::MotionAuthority::AnimationDriven);
    }
}

void CharacterTest::Update(float deltaTime) {
    if (!m_window) return;

    m_window->PollEvents();

    api::debug::DebugDraw::BeginFrame(deltaTime);

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

    // Cache yaw before any rotation update (for root motion translation)
    m_prevYaw = m_character.GetRotation();

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

    // Skeleton debug: draw bone lines (world space)
    if (m_skeleton && m_animator) {
        const auto& globalMatrices = m_animator->GetBoneGlobalMatrices();
        const size_t count = globalMatrices.size();
        if (count == m_skeleton->BoneCount()) {
            const math::Mat4 charTransform = m_character.GetTransform();
            const math::Vec4 green(0.2f, 1.0f, 0.3f, 1.0f);
            for (size_t i = 0; i < count; ++i) {
                const auto& bone = m_skeleton->GetBone(i);
                if (bone.parentIndex < 0) continue;
                const size_t p = static_cast<size_t>(bone.parentIndex);
                if (p >= count) continue;
                // Model-space positions: GLM mat4 is column-major, translation is column 3 → [3][0],[3][1],[3][2]
                const math::Vec3 modelA(globalMatrices[p][3][0], globalMatrices[p][3][1], globalMatrices[p][3][2]);
                const math::Vec3 modelB(globalMatrices[i][3][0], globalMatrices[i][3][1], globalMatrices[i][3][2]);
                const math::Vec4 worldA = charTransform * math::Vec4(modelA.x, modelA.y, modelA.z, 1.0f);
                const math::Vec4 worldB = charTransform * math::Vec4(modelB.x, modelB.y, modelB.z, 1.0f);
                api::debug::DebugDraw::AddLine(
                    math::Vec3(worldA.x, worldA.y, worldA.z),
                    math::Vec3(worldB.x, worldB.y, worldB.z),
                    green, 0.0f);
            }
        }
    }

    // Pivot debug: draw a big red dot at the actual pivot world position
    {
        const math::Mat4 charTransform = m_character.GetTransform();
        const math::Vec3 pivotModel = m_character.GetPivotOffset();
        const math::Vec4 pivotWorld4 = charTransform * math::Vec4(pivotModel.x, pivotModel.y, pivotModel.z, 1.0f);
        const math::Vec3 pivotWorld(pivotWorld4.x, pivotWorld4.y, pivotWorld4.z);
        const math::Vec4 red(1.0f, 0.1f, 0.1f, 1.0f);
        api::debug::DebugDraw::AddSphere(pivotWorld, 0.15f, red, 0.0f, 16);
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
