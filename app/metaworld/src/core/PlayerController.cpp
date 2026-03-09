/*
    Player Controller Implementation
*/
#include "PlayerController.h"
#include "../components/GameComponents.h"
#include <iostream>
#include <algorithm>

namespace Metaverse {

PlayerController::PlayerController(ECS::World* ecsWorld) 
    : m_ecsWorld(ecsWorld) {
    
    std::cout << "🎮 Player Controller initialized" << std::endl;
}

PlayerController::~PlayerController() {
    std::cout << "🎮 Player Controller destroyed" << std::endl;
}

void PlayerController::update(float deltaTime, ECS::Entity playerEntity) {
    if (!m_isPlayerEnabled || !m_ecsWorld->is_entity_valid(playerEntity)) {
        return;
    }

    // Update input
    updateInput(deltaTime);

    // Update movement
    updateMovement(deltaTime, playerEntity);

    // Update camera
    updateCamera(deltaTime, playerEntity);

    // Update physics
    updatePhysics(deltaTime, playerEntity);
}

void PlayerController::handleMovement(float deltaTime, ECS::Entity playerEntity) {
    updateMovement(deltaTime, playerEntity);
}

void PlayerController::handleCamera(float deltaTime, ECS::Entity playerEntity) {
    updateCamera(deltaTime, playerEntity);
}

void PlayerController::handleInteraction(float deltaTime, ECS::Entity playerEntity) {
    // Handle interaction input (E key, mouse clicks, etc.)
    if (isKeyPressed(GLFW_KEY_E)) {
        // Interaction logic here
        std::cout << "🔍 Player interaction triggered" << std::endl;
    }

    if (isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        // Left click interaction
        std::cout << "🖱️ Left click interaction" << std::endl;
    }

    if (isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
        // Right click interaction
        std::cout << "🖱️ Right click interaction" << std::endl;
    }
}

void PlayerController::setMovementSpeed(float speed) {
    m_movementSpeed = std::max(0.1f, speed);
    std::cout << "🏃 Movement speed set to " << m_movementSpeed << std::endl;
}

void PlayerController::setMouseSensitivity(float sensitivity) {
    m_mouseSensitivity = std::max(0.1f, sensitivity);
    std::cout << "🖱️ Mouse sensitivity set to " << m_mouseSensitivity << std::endl;
}

void PlayerController::setJumpHeight(float height) {
    m_jumpHeight = std::max(0.0f, height);
    std::cout << "🦘 Jump height set to " << m_jumpHeight << std::endl;
}

void PlayerController::setCameraHeight(float height) {
    m_cameraHeight = std::max(0.1f, height);
    std::cout << "📷 Camera height set to " << m_cameraHeight << std::endl;
}

void PlayerController::setCameraDistance(float distance) {
    m_cameraDistance = std::max(0.0f, distance);
    std::cout << "📷 Camera distance set to " << m_cameraDistance << std::endl;
}

void PlayerController::setCameraFOV(float fov) {
    m_cameraFOV = std::clamp(fov, 30.0f, 120.0f);
    std::cout << "📷 Camera FOV set to " << m_cameraFOV << std::endl;
}

void PlayerController::setPlayerEnabled(bool enabled) {
    m_isPlayerEnabled = enabled;
    std::cout << "🎮 Player " << (enabled ? "enabled" : "disabled") << std::endl;
}

bool PlayerController::isPlayerEnabled() const {
    return m_isPlayerEnabled;
}

void PlayerController::printPlayerInfo(ECS::Entity playerEntity) const {
    if (!m_ecsWorld->is_entity_valid(playerEntity)) {
        std::cout << "❌ Invalid player entity" << std::endl;
        return;
    }

    Vec3 position = getPlayerPosition(playerEntity);
    Vec3 rotation = getPlayerRotation(playerEntity);

    std::cout << "\n👤 === PLAYER INFO ===" << std::endl;
    std::cout << "Position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    std::cout << "Rotation: (" << rotation.x << ", " << rotation.y << ", " << rotation.z << ")" << std::endl;
    std::cout << "Velocity: (" << m_velocity.x << ", " << m_velocity.y << ", " << m_velocity.z << ")" << std::endl;
    std::cout << "Grounded: " << (m_isGrounded ? "Yes" : "No") << std::endl;
    std::cout << "Moving: " << (m_isMoving ? "Yes" : "No") << std::endl;
    std::cout << "Running: " << (m_isRunning ? "Yes" : "No") << std::endl;
    std::cout << "Speed: " << m_currentSpeed << std::endl;
    std::cout << "=====================\n" << std::endl;
}

void PlayerController::updateMovement(float deltaTime, ECS::Entity playerEntity) {
    // Calculate movement direction based on input
    Vec3 movementDirection = calculateMovementDirection();
    
    if (length(movementDirection) > 0.0f) {
        m_isMoving = true;
        m_currentSpeed = m_movementSpeed;
        
        // Apply running speed if shift is held
        if (isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
            m_currentSpeed *= 2.0f;
            m_isRunning = true;
        } else {
            m_isRunning = false;
        }
        
        // Apply movement
        applyMovement(playerEntity, movementDirection, deltaTime);
    } else {
        m_isMoving = false;
        m_isRunning = false;
        m_currentSpeed = 0.0f;
    }

    // Handle jumping
    if (isKeyPressed(GLFW_KEY_SPACE) && m_isGrounded && !m_isJumping) {
        handleJump(playerEntity);
    }

    // Handle crouching
    if (isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        handleCrouch(playerEntity);
    }
}

void PlayerController::updateCamera(float deltaTime, ECS::Entity playerEntity) {
    // Update camera rotation based on mouse input
    updateCameraRotation(deltaTime);
    
    // Update camera position
    updateCameraPosition(playerEntity);
    
    // Update camera transform
    updateCameraTransform(playerEntity);
}

void PlayerController::updatePhysics(float deltaTime, ECS::Entity playerEntity) {
    // Apply gravity
    applyGravity(playerEntity, deltaTime);
    
    // Check ground collision
    m_isGrounded = checkGroundCollision(playerEntity);
    
    // Update velocity
    Vec3 position = getPlayerPosition(playerEntity);
    position += m_velocity * deltaTime;
    setPlayerPosition(playerEntity, position);
}

void PlayerController::updateInput(float deltaTime) {
    // Update mouse delta
    Vec2 mouseDelta = getMouseDelta();
    m_mouseDelta = mouseDelta;
}

Vec3 PlayerController::calculateMovementDirection() const {
    Vec3 direction = Vec3(0.0f);
    
    // Forward/Backward (W/S)
    if (isKeyPressed(GLFW_KEY_W)) {
        direction.z -= 1.0f;
    }
    if (isKeyPressed(GLFW_KEY_S)) {
        direction.z += 1.0f;
    }
    
    // Left/Right (A/D)
    if (isKeyPressed(GLFW_KEY_A)) {
        direction.x -= 1.0f;
    }
    if (isKeyPressed(GLFW_KEY_D)) {
        direction.x += 1.0f;
    }
    
    // Normalize direction
    if (length(direction) > 0.0f) {
        direction = normalize(direction);
    }
    
    return direction;
}

void PlayerController::applyMovement(ECS::Entity playerEntity, const Vec3& direction, float deltaTime) {
    Vec3 position = getPlayerPosition(playerEntity);
    Vec3 rotation = getPlayerRotation(playerEntity);
    
    // Calculate movement in world space based on camera rotation
    float yaw = rotation.y;
    Vec3 forward = Vec3(sin(yaw), 0, cos(yaw));
    Vec3 right = cross(forward, Vec3(0, 1, 0));
    
    // Apply movement
    Vec3 movement = (forward * -direction.z + right * direction.x) * m_currentSpeed * deltaTime;
    position += movement;
    
    setPlayerPosition(playerEntity, position);
}

void PlayerController::applyGravity(ECS::Entity playerEntity, float deltaTime) {
    if (!m_isGrounded) {
        m_velocity.y += m_gravity * deltaTime;
    } else {
        m_velocity.y = 0.0f;
    }
}

void PlayerController::handleJump(ECS::Entity playerEntity) {
    if (m_isGrounded) {
        m_velocity.y = sqrt(2.0f * m_jumpHeight * -m_gravity);
        m_isJumping = true;
        std::cout << "🦘 Player jumped!" << std::endl;
    }
}

void PlayerController::handleCrouch(ECS::Entity playerEntity) {
    m_isCrouching = true;
    m_currentSpeed *= 0.5f; // Slow down when crouching
}

void PlayerController::updateCameraRotation(float deltaTime) {
    // Update yaw and pitch based on mouse movement
    m_yaw += m_mouseDelta.x * m_mouseSensitivity * deltaTime;
    m_pitch -= m_mouseDelta.y * m_mouseSensitivity * deltaTime;
    
    // Constrain pitch
    constrainPitch();
}

void PlayerController::updateCameraPosition(ECS::Entity playerEntity) {
    // Camera position is relative to player position
    // This will be handled in updateCameraTransform
}

void PlayerController::constrainPitch() {
    m_pitch = clamp(m_pitch, m_minPitch, m_maxPitch);
}

void PlayerController::updateCameraTransform(ECS::Entity playerEntity) {
    if (!m_ecsWorld->has_component<ECS::TransformComponent>(playerEntity)) {
        return;
    }
    
    auto& transform = m_ecsWorld->get_component<ECS::TransformComponent>(playerEntity);
    
    // Update player rotation (yaw only for movement)
    transform.set_rotation(Vec3(0.0f, m_yaw, 0.0f));
    
    // Update camera rotation if camera component exists
    if (m_ecsWorld->has_component<ECS::CameraComponent>(playerEntity)) {
        auto& camera = m_ecsWorld->get_component<ECS::CameraComponent>(playerEntity);
        // Camera rotation is handled by the camera component
    }
}

bool PlayerController::checkGroundCollision(ECS::Entity playerEntity) const {
    // Simple ground check - assume ground is at y = 0
    Vec3 position = getPlayerPosition(playerEntity);
    return position.y <= 0.1f; // Small threshold for ground detection
}

Vec3 PlayerController::getPlayerPosition(ECS::Entity playerEntity) const {
    if (!m_ecsWorld->has_component<ECS::TransformComponent>(playerEntity)) {
        return Vec3(0.0f);
    }
    
    const auto& transform = m_ecsWorld->get_component<ECS::TransformComponent>(playerEntity);
    return transform.position;
}

void PlayerController::setPlayerPosition(ECS::Entity playerEntity, const Vec3& position) {
    if (!m_ecsWorld->has_component<ECS::TransformComponent>(playerEntity)) {
        return;
    }
    
    auto& transform = m_ecsWorld->get_component<ECS::TransformComponent>(playerEntity);
    transform.set_position(position);
}

Vec3 PlayerController::getPlayerRotation(ECS::Entity playerEntity) const {
    if (!m_ecsWorld->has_component<ECS::TransformComponent>(playerEntity)) {
        return Vec3(0.0f);
    }
    
    const auto& transform = m_ecsWorld->get_component<ECS::TransformComponent>(playerEntity);
    return transform.rotation;
}

void PlayerController::setPlayerRotation(ECS::Entity playerEntity, const Vec3& rotation) {
    if (!m_ecsWorld->has_component<ECS::TransformComponent>(playerEntity)) {
        return;
    }
    
    auto& transform = m_ecsWorld->get_component<ECS::TransformComponent>(playerEntity);
    transform.set_rotation(rotation);
}

bool PlayerController::isKeyPressed(int key) const {
    // This would need access to the window to check key states
    // For now, return false as a placeholder
    return false;
}

bool PlayerController::isMouseButtonPressed(int button) const {
    // This would need access to the window to check mouse button states
    // For now, return false as a placeholder
    return false;
}

Vec2 PlayerController::getMousePosition() const {
    // This would need access to the window to get mouse position
    // For now, return (0,0) as a placeholder
    return Vec2(0.0f);
}

Vec2 PlayerController::getMouseDelta() {
    // This would need access to the window to get mouse delta
    // For now, return (0,0) as a placeholder
    return Vec2(0.0f);
}

float PlayerController::clamp(float value, float min, float max) const {
    return std::clamp(value, min, max);
}

Vec3 PlayerController::normalize(const Vec3& vector) const {
    float len = length(vector);
    if (len > 0.0f) {
        return vector / len;
    }
    return Vec3(0.0f);
}

float PlayerController::length(const Vec3& vector) const {
    return glm::length(vector);
}

Vec3 PlayerController::cross(const Vec3& a, const Vec3& b) const {
    return glm::cross(a, b);
}

float PlayerController::dot(const Vec3& a, const Vec3& b) const {
    return glm::dot(a, b);
}

} // namespace Metaverse
