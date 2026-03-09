/*
    Player Controller - Handles player input and movement
    
    This system manages player input and movement using the existing engine systems
    without modifying the core library.
*/
#ifndef PLAYER_CONTROLLER_H
#define PLAYER_CONTROLLER_H

#include <engine/ecs/ecs.h>
#include <engine/core/windows/windowGLFW.h>
#include <engine/common.h>
#include <memory>

namespace Metaverse {

// Forward declarations
class MetaverseGame;

class PlayerController {
public:
    PlayerController(ECS::World* ecsWorld);
    ~PlayerController();

    // Main update method
    void update(float deltaTime, ECS::Entity playerEntity);

    // Input handling
    void handleMovement(float deltaTime, ECS::Entity playerEntity);
    void handleCamera(float deltaTime, ECS::Entity playerEntity);
    void handleInteraction(float deltaTime, ECS::Entity playerEntity);

    // Movement settings
    void setMovementSpeed(float speed);
    void setMouseSensitivity(float sensitivity);
    void setJumpHeight(float height);

    // Camera settings
    void setCameraHeight(float height);
    void setCameraDistance(float distance);
    void setCameraFOV(float fov);

    // State management
    void setPlayerEnabled(bool enabled);
    bool isPlayerEnabled() const;

    // Debug
    void printPlayerInfo(ECS::Entity playerEntity) const;

private:
    // ECS world reference
    ECS::World* m_ecsWorld;

    // Movement settings
    float m_movementSpeed = 5.0f;
    float m_mouseSensitivity = 2.0f;
    float m_jumpHeight = 2.0f;
    float m_gravity = -9.81f;
    float m_groundCheckDistance = 0.1f;

    // Camera settings
    float m_cameraHeight = 1.8f;
    float m_cameraDistance = 0.0f;
    float m_cameraFOV = 70.0f;
    float m_minPitch = -89.0f;
    float m_maxPitch = 89.0f;

    // Input state
    bool m_isPlayerEnabled = true;
    bool m_isGrounded = true;
    bool m_isJumping = false;
    Vec3 m_velocity = Vec3(0.0f);
    Vec2 m_mouseDelta = Vec2(0.0f);
    Vec2 m_lastMousePosition = Vec2(0.0f);

    // Movement state
    bool m_isMoving = false;
    bool m_isRunning = false;
    bool m_isCrouching = false;
    float m_currentSpeed = 0.0f;

    // Camera state
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    bool m_firstMouse = true;

    // Internal methods
    void updateMovement(float deltaTime, ECS::Entity playerEntity);
    void updateCamera(float deltaTime, ECS::Entity playerEntity);
    void updatePhysics(float deltaTime, ECS::Entity playerEntity);
    void updateInput(float deltaTime);

    // Input helpers
    bool isKeyPressed(int key) const;
    bool isMouseButtonPressed(int button) const;
    Vec2 getMousePosition() const;
    Vec2 getMouseDelta();

    // Movement helpers
    Vec3 calculateMovementDirection() const;
    void applyMovement(ECS::Entity playerEntity, const Vec3& direction, float deltaTime);
    void applyGravity(ECS::Entity playerEntity, float deltaTime);
    void handleJump(ECS::Entity playerEntity);
    void handleCrouch(ECS::Entity playerEntity);

    // Camera helpers
    void updateCameraRotation(float deltaTime);
    void updateCameraPosition(ECS::Entity playerEntity);
    void constrainPitch();
    void updateCameraTransform(ECS::Entity playerEntity);

    // Physics helpers
    bool checkGroundCollision(ECS::Entity playerEntity) const;
    Vec3 getPlayerPosition(ECS::Entity playerEntity) const;
    void setPlayerPosition(ECS::Entity playerEntity, const Vec3& position);
    Vec3 getPlayerRotation(ECS::Entity playerEntity) const;
    void setPlayerRotation(ECS::Entity playerEntity, const Vec3& rotation);

    // Utility methods
    float clamp(float value, float min, float max) const;
    Vec3 normalize(const Vec3& vector) const;
    float length(const Vec3& vector) const;
    Vec3 cross(const Vec3& a, const Vec3& b) const;
    float dot(const Vec3& a, const Vec3& b) const;
};

} // namespace Metaverse

#endif // PLAYER_CONTROLLER_H
