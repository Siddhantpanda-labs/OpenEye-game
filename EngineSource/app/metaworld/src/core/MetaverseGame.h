/*
    Metaverse Game - Main Application Class
    
    This is the main game application that uses the Vulkan Engine library
    without modifying it. All game logic lives in this layer.
*/
#ifndef METAVERSE_GAME_H
#define METAVERSE_GAME_H

#include <engine/ecs/ecs.h>
#include <engine/ecs/ecs_scene_bridge.h>
#include <engine/core/windows/windowGLFW.h>
#include <engine/systems/renderers/forward.h>
#include <engine/core/scene/scene.h>
#include <engine/common.h>
#include <memory>
#include <string>

namespace Metaverse {

// Forward declarations
class CityGenerator;
class PlayerController;

class MetaverseGame {
public:
    MetaverseGame();
    ~MetaverseGame();

    // Main game lifecycle
    bool initialize();
    void run();
    void shutdown();

    // Game state management
    void setGameState(const std::string& state);
    std::string getCurrentGameState() const;

    // World management
    void loadCityWorld();
    void unloadCityWorld();
    bool isCityWorldLoaded() const;

    // Player management
    void spawnPlayer();
    void despawnPlayer();
    bool isPlayerSpawned() const;

    // Debug and profiling
    void printGameStatistics() const;
    void toggleDebugMode();

private:
    // Core engine systems (using existing library - NO MODIFICATIONS)
    std::unique_ptr<Core::WindowGLFW> m_window;
    std::unique_ptr<Systems::ForwardRenderer> m_renderer;
    std::unique_ptr<Core::Scene> m_legacyScene;
    
    // ECS system (new integration)
    std::unique_ptr<ECS::World> m_ecsWorld;
    std::unique_ptr<ECS::ECSSceneBridge> m_bridge;
    
    // Game systems
    std::unique_ptr<CityGenerator> m_cityGenerator;
    std::unique_ptr<PlayerController> m_playerController;
    
    // Game state
    std::string m_currentGameState = "menu";
    bool m_isInitialized = false;
    bool m_isRunning = false;
    bool m_debugMode = false;
    
    // Player state
    ECS::Entity m_playerEntity = ECS::NULL_ENTITY;
    bool m_playerSpawned = false;
    
    // World state
    bool m_cityWorldLoaded = false;
    
    // Performance tracking
    float m_lastFrameTime = 0.0f;
    float m_frameTimeAccumulator = 0.0f;
    int m_frameCount = 0;
    
    // Internal methods
    void initializeEngine();
    void initializeECS();
    void initializeGameSystems();
    void updateGame(float deltaTime);
    void renderGame();
    void handleInput(float deltaTime);
    void updatePerformanceStats(float deltaTime);
    
    // Game state handlers
    void handleMenuState(float deltaTime);
    void handlePlayingState(float deltaTime);
    void handlePausedState(float deltaTime);
    
    // Utility methods
    void logGameEvent(const std::string& event);
    void handleWindowEvents();
    void updateWindowTitle();
};

} // namespace Metaverse

#endif // METAVERSE_GAME_H
