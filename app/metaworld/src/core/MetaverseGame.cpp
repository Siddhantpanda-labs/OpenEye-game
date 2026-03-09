/*
    Metaverse Game - Main Application Implementation
*/
#include "MetaverseGame.h"
#include "../world/CityGenerator.h"
#include "PlayerController.h"
#include <iostream>
#include <chrono>

namespace Metaverse {

MetaverseGame::MetaverseGame() {
    std::cout << "🎮 Initializing Metaverse Game..." << std::endl;
}

MetaverseGame::~MetaverseGame() {
    shutdown();
}

bool MetaverseGame::initialize() {
    try {
        std::cout << "🚀 Starting Metaverse Game initialization..." << std::endl;
        
        // Initialize engine systems
        initializeEngine();
        
        // Initialize ECS system
        initializeECS();
        
        // Initialize game systems
        initializeGameSystems();
        
        // Set initial game state
        setGameState("playing");
        
        m_isInitialized = true;
        std::cout << "✅ Metaverse Game initialized successfully!" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Failed to initialize Metaverse Game: " << e.what() << std::endl;
        return false;
    }
}

void MetaverseGame::run() {
    if (!m_isInitialized) {
        std::cerr << "❌ Game not initialized!" << std::endl;
        return;
    }
    
    std::cout << "🎮 Starting Metaverse Game main loop..." << std::endl;
    m_isRunning = true;
    
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (m_isRunning && !m_window->get_window_should_close()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;
        
        // Clamp delta time to prevent large jumps
        deltaTime = std::min(deltaTime, 0.016f); // Max 60 FPS
        
        // Update performance stats
        updatePerformanceStats(deltaTime);
        
        // Handle input
        handleInput(deltaTime);
        
        // Update game logic
        updateGame(deltaTime);
        
        // Render game
        renderGame();
        
        // Handle window events
        handleWindowEvents();
        
        // Update window title with FPS
        updateWindowTitle();
    }
    
    std::cout << "🏁 Metaverse Game main loop ended." << std::endl;
}

void MetaverseGame::shutdown() {
    if (!m_isInitialized) {
        return;
    }
    
    std::cout << "🛑 Shutting down Metaverse Game..." << std::endl;
    
    // Unload world
    unloadCityWorld();
    
    // Shutdown game systems
    m_playerController.reset();
    m_cityGenerator.reset();
    
    // Shutdown ECS
    m_bridge.reset();
    m_ecsWorld.reset();
    
    // Shutdown engine
    m_renderer.reset();
    m_legacyScene.reset();
    m_window.reset();
    
    m_isInitialized = false;
    m_isRunning = false;
    
    std::cout << "✅ Metaverse Game shutdown complete." << std::endl;
}

void MetaverseGame::setGameState(const std::string& state) {
    if (m_currentGameState != state) {
        std::cout << "🔄 Game state changed: " << m_currentGameState << " -> " << state << std::endl;
        m_currentGameState = state;
        logGameEvent("State changed to: " + state);
    }
}

std::string MetaverseGame::getCurrentGameState() const {
    return m_currentGameState;
}

void MetaverseGame::loadCityWorld() {
    if (m_cityWorldLoaded) {
        return;
    }
    
    std::cout << "🏙️ Loading city world..." << std::endl;
    
    // Generate city using the city generator
    m_cityGenerator->generateBasicCity(*m_ecsWorld);
    
    // Spawn player
    spawnPlayer();
    
    m_cityWorldLoaded = true;
    logGameEvent("City world loaded");
    
    std::cout << "✅ City world loaded successfully!" << std::endl;
}

void MetaverseGame::unloadCityWorld() {
    if (!m_cityWorldLoaded) {
        return;
    }
    
    std::cout << "🏙️ Unloading city world..." << std::endl;
    
    // Despawn player
    despawnPlayer();
    
    // Clear ECS world
    m_ecsWorld->clear_scene();
    
    m_cityWorldLoaded = false;
    logGameEvent("City world unloaded");
    
    std::cout << "✅ City world unloaded." << std::endl;
}

bool MetaverseGame::isCityWorldLoaded() const {
    return m_cityWorldLoaded;
}

void MetaverseGame::spawnPlayer() {
    if (m_playerSpawned) {
        return;
    }
    
    std::cout << "👤 Spawning player..." << std::endl;
    
    // Create player entity
    m_playerEntity = m_ecsWorld->create_entity();
    
    // Add transform component (spawn at origin for now)
    m_ecsWorld->add_component<ECS::TransformComponent>(m_playerEntity, 
        Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f));
    
    // Add camera component
    m_ecsWorld->add_component<ECS::CameraComponent>(m_playerEntity, 70.0f, 0.1f, 1000.0f);
    
    // Add player-specific component
    m_ecsWorld->add_component<PlayerComponent>(m_playerEntity, "Player", 100, 0);
    
    m_playerSpawned = true;
    logGameEvent("Player spawned");
    
    std::cout << "✅ Player spawned successfully!" << std::endl;
}

void MetaverseGame::despawnPlayer() {
    if (!m_playerSpawned) {
        return;
    }
    
    std::cout << "👤 Despawning player..." << std::endl;
    
    // Destroy player entity
    m_ecsWorld->destroy_entity(m_playerEntity);
    m_playerEntity = ECS::NULL_ENTITY;
    
    m_playerSpawned = false;
    logGameEvent("Player despawned");
    
    std::cout << "✅ Player despawned." << std::endl;
}

bool MetaverseGame::isPlayerSpawned() const {
    return m_playerSpawned;
}

void MetaverseGame::printGameStatistics() const {
    std::cout << "\n📊 === METAVERSE GAME STATISTICS ===" << std::endl;
    std::cout << "Game State: " << m_currentGameState << std::endl;
    std::cout << "City World Loaded: " << (m_cityWorldLoaded ? "Yes" : "No") << std::endl;
    std::cout << "Player Spawned: " << (m_playerSpawned ? "Yes" : "No") << std::endl;
    std::cout << "Debug Mode: " << (m_debugMode ? "On" : "Off") << std::endl;
    
    if (m_ecsWorld) {
        std::cout << "ECS Entities: " << m_ecsWorld->get_entity_count() << std::endl;
    }
    
    std::cout << "Average FPS: " << (m_frameTimeAccumulator > 0 ? 1.0f / (m_frameTimeAccumulator / m_frameCount) : 0) << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

void MetaverseGame::toggleDebugMode() {
    m_debugMode = !m_debugMode;
    std::cout << "🐛 Debug mode: " << (m_debugMode ? "ON" : "OFF") << std::endl;
}

void MetaverseGame::initializeEngine() {
    std::cout << "🔧 Initializing engine systems..." << std::endl;
    
    // Create window
    m_window = std::make_unique<Core::WindowGLFW>("Metaverse Game", 1920, 1080);
    m_window->init();
    
    // Create renderer
    m_renderer = std::make_unique<Systems::ForwardRenderer>(m_window);
    
    // Create legacy scene
    m_legacyScene = std::make_unique<Core::Scene>();
    
    std::cout << "✅ Engine systems initialized." << std::endl;
}

void MetaverseGame::initializeECS() {
    std::cout << "🏗️ Initializing ECS system..." << std::endl;
    
    // Create ECS world
    m_ecsWorld = std::make_unique<ECS::World>();
    
    // Create bridge between ECS and legacy scene
    m_bridge = std::make_unique<ECS::ECSSceneBridge>(m_ecsWorld.get(), m_legacyScene.get());
    
    std::cout << "✅ ECS system initialized." << std::endl;
}

void MetaverseGame::initializeGameSystems() {
    std::cout << "🎮 Initializing game systems..." << std::endl;
    
    // Create city generator
    m_cityGenerator = std::make_unique<CityGenerator>();
    
    // Create player controller
    m_playerController = std::make_unique<PlayerController>(m_ecsWorld.get());
    
    std::cout << "✅ Game systems initialized." << std::endl;
}

void MetaverseGame::updateGame(float deltaTime) {
    // Update ECS systems
    if (m_ecsWorld) {
        m_ecsWorld->update_systems(deltaTime);
    }
    
    // Update player controller
    if (m_playerController && m_playerSpawned) {
        m_playerController->update(deltaTime, m_playerEntity);
    }
    
    // Sync ECS to legacy scene for rendering
    if (m_bridge) {
        m_bridge->sync_ecs_to_scene();
    }
    
    // Handle game state
    if (m_currentGameState == "playing") {
        handlePlayingState(deltaTime);
    } else if (m_currentGameState == "menu") {
        handleMenuState(deltaTime);
    } else if (m_currentGameState == "paused") {
        handlePausedState(deltaTime);
    }
}

void MetaverseGame::renderGame() {
    if (m_renderer && m_legacyScene) {
        m_renderer->render(m_legacyScene.get());
    }
}

void MetaverseGame::handleInput(float deltaTime) {
    // Handle window input
    m_window->poll_events();
    
    // Handle game-specific input
    if (m_window->get_key_state(GLFW_KEY_ESCAPE, GLFW_PRESS)) {
        if (m_currentGameState == "playing") {
            setGameState("paused");
        } else if (m_currentGameState == "paused") {
            setGameState("playing");
        }
    }
    
    if (m_window->get_key_state(GLFW_KEY_F1, GLFW_PRESS)) {
        toggleDebugMode();
    }
    
    if (m_window->get_key_state(GLFW_KEY_F2, GLFW_PRESS)) {
        printGameStatistics();
    }
}

void MetaverseGame::updatePerformanceStats(float deltaTime) {
    m_frameTimeAccumulator += deltaTime;
    m_frameCount++;
    
    // Update every second
    if (m_frameTimeAccumulator >= 1.0f) {
        m_frameTimeAccumulator = 0.0f;
        m_frameCount = 0;
    }
}

void MetaverseGame::handleMenuState(float deltaTime) {
    // Menu logic here
}

void MetaverseGame::handlePlayingState(float deltaTime) {
    // Main gameplay logic here
}

void MetaverseGame::handlePausedState(float deltaTime) {
    // Paused state logic here
}

void MetaverseGame::logGameEvent(const std::string& event) {
    if (m_debugMode) {
        std::cout << "📝 Game Event: " << event << std::endl;
    }
}

void MetaverseGame::handleWindowEvents() {
    // Handle window resize, etc.
}

void MetaverseGame::updateWindowTitle() {
    if (m_frameTimeAccumulator > 0 && m_frameCount > 0) {
        float fps = 1.0f / (m_frameTimeAccumulator / m_frameCount);
        std::string title = "Metaverse Game - FPS: " + std::to_string(static_cast<int>(fps));
        m_window->set_title(title.c_str());
    }
}

} // namespace Metaverse
