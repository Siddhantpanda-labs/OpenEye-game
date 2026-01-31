#pragma once

#include "ThirdPersonCamera.h"
#include "CharacterController.h"
#include "api/platform/Window.h"
#include "api/render/RenderScene.h"
#include "api/render/Mesh.h"
#include "api/render/Material.h"
#include "api/render/RenderPipeline.h"
#include "api/render/Renderer.h"
#include "api/animation/Animator.h"
#include "api/animation/Skeleton.h"
#include "api/animation/AnimationClip.h"
#include "api/animation/StateMachine.h"
#include "api/assets/ModelAsset.h"
#include "eyelib/assets/ShaderWatcher.h"
#include "api/physics/PhysicsWorld.h"
#include "api/physics/CharacterController.h"
#include <memory>
#include <string>
#include <vector>

namespace eyelib {
namespace test {

/**
 * @brief Main character test application.
 * 
 * Loads animated character model, sets up third-person camera,
 * handles input, and renders the scene.
 */
class CharacterTest {
public:
    CharacterTest();
    ~CharacterTest();

    /**
     * @brief Initialize the test application.
     * 
     * @param windowWidth Window width
     * @param windowHeight Window height
     * @return true if initialization successful
     */
    bool Initialize(int windowWidth, int windowHeight);

    /**
     * @brief Update test application (called every frame).
     * 
     * @param deltaTime Frame delta time in seconds
     */
    void Update(float deltaTime);

    /**
     * @brief Render the scene.
     */
    void Render();

    /**
     * @brief Check if test should close.
     */
    bool ShouldClose() const;

    /**
     * @brief Shutdown and cleanup.
     */
    void Shutdown();

    /**
     * @brief Load character model from file.
     * 
     * @param modelPath Path to model file (FBX/GLTF)
     * @return true if loaded successfully
     */
    bool LoadCharacterModel(const std::string& modelPath);

    /**
     * @brief Load animation clips.
     * 
     * @param idlePath Path to idle animation
     * @param walkPath Path to walk animation (optional)
     * @param runPath Path to run animation (optional)
     */
    void LoadAnimations(const std::string& idlePath,
                       const std::string& walkPath = "",
                       const std::string& runPath = "");

private:
    void ProcessInput(float deltaTime);
    void UpdateCamera(float deltaTime);
    void UpdateCharacter(float deltaTime);
    void SetupScene();
    void RenderPanels();

    // Shader file watcher: sync root assets/shaders -> runtime assets/shaders
    std::unique_ptr<eyelib::assets::ShaderWatcher> m_shaderWatcher;

    // Window and rendering
    std::shared_ptr<api::Window> m_window;
    std::shared_ptr<api::Renderer> m_renderer;
    std::shared_ptr<api::RenderPipeline> m_pipeline;
    api::RenderScene m_scene;

    // Ground plane (visual; collision via physics)
    std::shared_ptr<api::Mesh> m_groundMesh;
    std::shared_ptr<api::Material> m_groundMaterial;
    static constexpr float k_groundY = 0.0f;

    // Physics: gravity, ground collider, character collider (model falls and lands on ground)
    api::physics::PhysicsWorld m_physicsWorld;
    api::physics::CharacterControllerHandle m_physicsCharacterHandle = 0;
    static constexpr float k_spawnHeight = 3.0f;   // spawn above ground, then fall
    static constexpr float k_physicsCharRadius = 0.4f;
    static constexpr float k_physicsCharHeight = 1.6f;

    // Character
    std::shared_ptr<api::ModelAsset> m_characterModel;
    std::shared_ptr<api::Mesh> m_characterMesh;
    std::shared_ptr<api::Material> m_characterMaterial;
    std::shared_ptr<api::Skeleton> m_skeleton;
    std::shared_ptr<api::Animator> m_animator;
    
    // Animation clips
    std::shared_ptr<api::AnimationClip> m_idleClip;
    std::shared_ptr<api::AnimationClip> m_walkClip;
    std::shared_ptr<api::AnimationClip> m_runClip;

    // State machine for walk/run transitions (when both clips loaded)
    std::shared_ptr<api::StateMachine> m_stateMachine;

    // Controllers
    ThirdPersonCamera m_camera;
    CharacterController m_character;

    // Input state
    bool m_mouseRightPressed = false;
    bool m_mouseRightPressedLast = false;
    math::Vec3 m_moveDirWorld{0.0f, 0.0f, 0.0f};

    // Window dimensions
    int m_windowWidth = 1280;
    int m_windowHeight = 720;

    bool m_shutdownDone = false;
};

} // namespace test
} // namespace eyelib
