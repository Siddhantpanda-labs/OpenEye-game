/*
    Game-specific components for the Metaverse Game
    
    These components extend the ECS system with game-specific functionality
    without modifying the core engine library.
*/
#ifndef GAME_COMPONENTS_H
#define GAME_COMPONENTS_H

#include <engine/ecs/ecs.h>
#include <engine/common.h>
#include <string>
#include <vector>

namespace Metaverse {

// Player component for game-specific player data
struct PlayerComponent {
    std::string playerName = "Player";
    int health = 100;
    int maxHealth = 100;
    int score = 0;
    int level = 1;
    float experience = 0.0f;
    
    // Game state
    bool isInArena = false;
    std::string currentArena = "";
    bool isAlive = true;
    
    // Movement
    float movementSpeed = 5.0f;
    float jumpHeight = 2.0f;
    bool isGrounded = true;
    
    // Inventory
    std::vector<std::string> weapons;
    std::string currentWeapon = "";
    int ammo = 30;
    int maxAmmo = 30;
    
    PlayerComponent() = default;
    
    PlayerComponent(const std::string& name, int hp, int scr)
        : playerName(name), health(hp), maxHealth(hp), score(scr) {}
    
    // Utility methods
    void takeDamage(int damage) {
        health = std::max(0, health - damage);
        if (health <= 0) {
            isAlive = false;
        }
    }
    
    void heal(int amount) {
        health = std::min(maxHealth, health + amount);
    }
    
    void addScore(int points) {
        score += points;
    }
    
    void addExperience(float exp) {
        experience += exp;
        // Simple leveling system
        if (experience >= 100.0f) {
            level++;
            experience = 0.0f;
            maxHealth += 10;
            health = maxHealth; // Full heal on level up
        }
    }
    
    // Serialization support
    template<typename Archive>
    void serialize(Archive& archive) {
        archive(playerName, health, maxHealth, score, level, experience,
                isInArena, currentArena, isAlive, movementSpeed, jumpHeight,
                isGrounded, weapons, currentWeapon, ammo, maxAmmo);
    }
};

// Weapon component for weapons and shooting mechanics
struct WeaponComponent {
    std::string weaponType = "pistol";
    std::string weaponName = "Basic Pistol";
    
    // Weapon stats
    int damage = 25;
    float fireRate = 0.1f; // Time between shots
    float range = 100.0f;
    float accuracy = 0.95f;
    int ammo = 12;
    int maxAmmo = 12;
    float reloadTime = 2.0f;
    
    // Weapon state
    bool isEquipped = false;
    bool isReloading = false;
    float lastFireTime = 0.0f;
    float reloadStartTime = 0.0f;
    
    // Visual/audio
    std::string modelPath = "";
    std::string fireSound = "";
    std::string reloadSound = "";
    std::string muzzleFlashEffect = "";
    
    WeaponComponent() = default;
    
    WeaponComponent(const std::string& type, const std::string& name, int dmg, float rate)
        : weaponType(type), weaponName(name), damage(dmg), fireRate(rate) {}
    
    // Weapon methods
    bool canFire(float currentTime) const {
        return !isReloading && ammo > 0 && (currentTime - lastFireTime) >= fireRate;
    }
    
    void fire(float currentTime) {
        if (canFire(currentTime)) {
            ammo--;
            lastFireTime = currentTime;
        }
    }
    
    void startReload(float currentTime) {
        if (!isReloading && ammo < maxAmmo) {
            isReloading = true;
            reloadStartTime = currentTime;
        }
    }
    
    void updateReload(float currentTime) {
        if (isReloading && (currentTime - reloadStartTime) >= reloadTime) {
            ammo = maxAmmo;
            isReloading = false;
        }
    }
    
    float getReloadProgress(float currentTime) const {
        if (!isReloading) return 1.0f;
        return std::min(1.0f, (currentTime - reloadStartTime) / reloadTime);
    }
    
    // Serialization support
    template<typename Archive>
    void serialize(Archive& archive) {
        archive(weaponType, weaponName, damage, fireRate, range, accuracy,
                ammo, maxAmmo, reloadTime, isEquipped, isReloading,
                lastFireTime, reloadStartTime, modelPath, fireSound,
                reloadSound, muzzleFlashEffect);
    }
};

// Arena component for shooting game locations
struct ArenaComponent {
    std::string arenaType = "office";
    std::string arenaName = "Office Building";
    
    // Arena settings
    int maxPlayers = 8;
    float timeLimit = 300.0f; // 5 minutes
    float currentTime = 0.0f;
    bool isActive = false;
    bool isCompleted = false;
    
    // Spawn points
    std::vector<Vec3> spawnPoints;
    std::vector<Vec3> targetPositions;
    
    // Scoring
    int targetScore = 50;
    int headshotBonus = 25;
    int timeBonus = 10;
    
    // Players in arena
    std::vector<ECS::Entity> players;
    std::vector<ECS::Entity> targets;
    
    ArenaComponent() = default;
    
    ArenaComponent(const std::string& type, const std::string& name, int maxP, float time)
        : arenaType(type), arenaName(name), maxPlayers(maxP), timeLimit(time) {}
    
    // Arena methods
    void addSpawnPoint(const Vec3& position) {
        spawnPoints.push_back(position);
    }
    
    void addTargetPosition(const Vec3& position) {
        targetPositions.push_back(position);
    }
    
    Vec3 getRandomSpawnPoint() const {
        if (spawnPoints.empty()) {
            return Vec3(0.0f, 0.0f, 0.0f);
        }
        return spawnPoints[rand() % spawnPoints.size()];
    }
    
    void startArena() {
        isActive = true;
        currentTime = 0.0f;
        isCompleted = false;
    }
    
    void endArena() {
        isActive = false;
        isCompleted = true;
    }
    
    void updateArena(float deltaTime) {
        if (isActive && !isCompleted) {
            currentTime += deltaTime;
            if (currentTime >= timeLimit) {
                endArena();
            }
        }
    }
    
    float getTimeRemaining() const {
        return std::max(0.0f, timeLimit - currentTime);
    }
    
    float getProgress() const {
        return currentTime / timeLimit;
    }
    
    // Serialization support
    template<typename Archive>
    void serialize(Archive& archive) {
        archive(arenaType, arenaName, maxPlayers, timeLimit, currentTime,
                isActive, isCompleted, spawnPoints, targetPositions,
                targetScore, headshotBonus, timeBonus);
    }
};

// Target component for shooting targets
struct TargetComponent {
    std::string targetType = "enemy";
    std::string targetName = "Enemy";
    
    // Target stats
    int health = 100;
    int maxHealth = 100;
    int scoreValue = 50;
    bool isAlive = true;
    
    // AI behavior
    std::string behavior = "static"; // static, patrol, chase, attack
    float detectionRange = 15.0f;
    float attackRange = 5.0f;
    float moveSpeed = 3.0f;
    
    // Target state
    bool isVisible = true;
    bool isShootable = true;
    float lastHitTime = 0.0f;
    Vec3 lastKnownPlayerPosition = Vec3(0.0f);
    
    TargetComponent() = default;
    
    TargetComponent(const std::string& type, const std::string& name, int hp, int score)
        : targetType(type), targetName(name), health(hp), maxHealth(hp), scoreValue(score) {}
    
    // Target methods
    void takeDamage(int damage) {
        health = std::max(0, health - damage);
        if (health <= 0) {
            isAlive = false;
        }
    }
    
    void heal(int amount) {
        health = std::min(maxHealth, health + amount);
    }
    
    bool isInRange(const Vec3& position, float range) const {
        // This would need the transform component to calculate distance
        return true; // Placeholder
    }
    
    // Serialization support
    template<typename Archive>
    void serialize(Archive& archive) {
        archive(targetType, targetName, health, maxHealth, scoreValue,
                isAlive, behavior, detectionRange, attackRange, moveSpeed,
                isVisible, isShootable, lastHitTime, lastKnownPlayerPosition);
    }
};

// Building component for city buildings
struct BuildingComponent {
    std::string buildingType = "office";
    std::string buildingName = "Office Building";
    
    // Building properties
    int floors = 3;
    float height = 15.0f;
    float width = 20.0f;
    float depth = 20.0f;
    
    // Building state
    bool isEnterable = true;
    bool hasArena = false;
    std::string arenaType = "";
    bool isLighted = true;
    
    // Building data
    std::string modelPath = "";
    std::string texturePath = "";
    std::vector<Vec3> entrancePoints;
    std::vector<Vec3> windowPositions;
    
    BuildingComponent() = default;
    
    BuildingComponent(const std::string& type, const std::string& name, int f, float h)
        : buildingType(type), buildingName(name), floors(f), height(h) {}
    
    // Building methods
    void addEntrance(const Vec3& position) {
        entrancePoints.push_back(position);
    }
    
    void addWindow(const Vec3& position) {
        windowPositions.push_back(position);
    }
    
    bool canEnter() const {
        return isEnterable && !entrancePoints.empty();
    }
    
    Vec3 getNearestEntrance(const Vec3& position) const {
        if (entrancePoints.empty()) {
            return position;
        }
        
        Vec3 nearest = entrancePoints[0];
        float minDistance = glm::length(position - nearest);
        
        for (const auto& entrance : entrancePoints) {
            float distance = glm::length(position - entrance);
            if (distance < minDistance) {
                minDistance = distance;
                nearest = entrance;
            }
        }
        
        return nearest;
    }
    
    // Serialization support
    template<typename Archive>
    void serialize(Archive& archive) {
        archive(buildingType, buildingName, floors, height, width, depth,
                isEnterable, hasArena, arenaType, isLighted, modelPath,
                texturePath, entrancePoints, windowPositions);
    }
};

// Street component for city streets and roads
struct StreetComponent {
    std::string streetType = "main";
    std::string streetName = "Main Street";
    
    // Street properties
    float width = 8.0f;
    float length = 100.0f;
    bool hasSidewalk = true;
    bool hasStreetLights = true;
    
    // Street data
    std::vector<Vec3> waypoints;
    std::vector<Vec3> streetLightPositions;
    std::vector<Vec3> sidewalkPositions;
    
    StreetComponent() = default;
    
    StreetComponent(const std::string& type, const std::string& name, float w, float l)
        : streetType(type), streetName(name), width(w), length(l) {}
    
    // Street methods
    void addWaypoint(const Vec3& position) {
        waypoints.push_back(position);
    }
    
    void addStreetLight(const Vec3& position) {
        streetLightPositions.push_back(position);
    }
    
    void addSidewalk(const Vec3& position) {
        sidewalkPositions.push_back(position);
    }
    
    // Serialization support
    template<typename Archive>
    void serialize(Archive& archive) {
        archive(streetType, streetName, width, length, hasSidewalk,
                hasStreetLights, waypoints, streetLightPositions, sidewalkPositions);
    }
};

} // namespace Metaverse

#endif // GAME_COMPONENTS_H
