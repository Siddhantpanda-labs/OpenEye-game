/*
    Game Components Implementation
*/
#include "GameComponents.h"
#include <iostream>

namespace Metaverse {

// PlayerComponent implementation
PlayerComponent::PlayerComponent() = default;

PlayerComponent::PlayerComponent(const std::string& name, int hp, int scr)
    : playerName(name), health(hp), maxHealth(hp), score(scr) {}

void PlayerComponent::takeDamage(int damage) {
    health = std::max(0, health - damage);
    if (health <= 0) {
        isAlive = false;
        std::cout << "💀 Player " << playerName << " has died!" << std::endl;
    } else {
        std::cout << "💔 Player " << playerName << " took " << damage << " damage. Health: " << health << std::endl;
    }
}

void PlayerComponent::heal(int amount) {
    health = std::min(maxHealth, health + amount);
    std::cout << "💚 Player " << playerName << " healed " << amount << " health. Health: " << health << std::endl;
}

void PlayerComponent::addScore(int points) {
    score += points;
    std::cout << "⭐ Player " << playerName << " scored " << points << " points! Total: " << score << std::endl;
}

void PlayerComponent::addExperience(float exp) {
    experience += exp;
    std::cout << "📈 Player " << playerName << " gained " << exp << " experience!" << std::endl;
    
    // Simple leveling system
    if (experience >= 100.0f) {
        level++;
        experience = 0.0f;
        maxHealth += 10;
        health = maxHealth; // Full heal on level up
        std::cout << "🎉 Player " << playerName << " leveled up to level " << level << "!" << std::endl;
    }
}

// WeaponComponent implementation
WeaponComponent::WeaponComponent() = default;

WeaponComponent::WeaponComponent(const std::string& type, const std::string& name, int dmg, float rate)
    : weaponType(type), weaponName(name), damage(dmg), fireRate(rate) {}

bool WeaponComponent::canFire(float currentTime) const {
    return !isReloading && ammo > 0 && (currentTime - lastFireTime) >= fireRate;
}

void WeaponComponent::fire(float currentTime) {
    if (canFire(currentTime)) {
        ammo--;
        lastFireTime = currentTime;
        std::cout << "🔫 " << weaponName << " fired! Ammo: " << ammo << "/" << maxAmmo << std::endl;
    } else if (ammo <= 0) {
        std::cout << "🔫 " << weaponName << " is out of ammo!" << std::endl;
    } else if (isReloading) {
        std::cout << "🔫 " << weaponName << " is reloading..." << std::endl;
    } else {
        std::cout << "🔫 " << weaponName << " fire rate too high!" << std::endl;
    }
}

void WeaponComponent::startReload(float currentTime) {
    if (!isReloading && ammo < maxAmmo) {
        isReloading = true;
        reloadStartTime = currentTime;
        std::cout << "🔄 " << weaponName << " started reloading..." << std::endl;
    }
}

void WeaponComponent::updateReload(float currentTime) {
    if (isReloading && (currentTime - reloadStartTime) >= reloadTime) {
        ammo = maxAmmo;
        isReloading = false;
        std::cout << "✅ " << weaponName << " reloaded! Ammo: " << ammo << "/" << maxAmmo << std::endl;
    }
}

float WeaponComponent::getReloadProgress(float currentTime) const {
    if (!isReloading) return 1.0f;
    return std::min(1.0f, (currentTime - reloadStartTime) / reloadTime);
}

// ArenaComponent implementation
ArenaComponent::ArenaComponent() = default;

ArenaComponent::ArenaComponent(const std::string& type, const std::string& name, int maxP, float time)
    : arenaType(type), arenaName(name), maxPlayers(maxP), timeLimit(time) {}

void ArenaComponent::addSpawnPoint(const Vec3& position) {
    spawnPoints.push_back(position);
    std::cout << "📍 Added spawn point to " << arenaName << " at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

void ArenaComponent::addTargetPosition(const Vec3& position) {
    targetPositions.push_back(position);
    std::cout << "🎯 Added target position to " << arenaName << " at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

Vec3 ArenaComponent::getRandomSpawnPoint() const {
    if (spawnPoints.empty()) {
        std::cout << "⚠️ No spawn points available in " << arenaName << std::endl;
        return Vec3(0.0f, 0.0f, 0.0f);
    }
    return spawnPoints[rand() % spawnPoints.size()];
}

void ArenaComponent::startArena() {
    isActive = true;
    currentTime = 0.0f;
    isCompleted = false;
    std::cout << "🏁 Arena " << arenaName << " started!" << std::endl;
}

void ArenaComponent::endArena() {
    isActive = false;
    isCompleted = true;
    std::cout << "🏁 Arena " << arenaName << " ended!" << std::endl;
}

void ArenaComponent::updateArena(float deltaTime) {
    if (isActive && !isCompleted) {
        currentTime += deltaTime;
        if (currentTime >= timeLimit) {
            endArena();
        }
    }
}

float ArenaComponent::getTimeRemaining() const {
    return std::max(0.0f, timeLimit - currentTime);
}

float ArenaComponent::getProgress() const {
    return currentTime / timeLimit;
}

// TargetComponent implementation
TargetComponent::TargetComponent() = default;

TargetComponent::TargetComponent(const std::string& type, const std::string& name, int hp, int score)
    : targetType(type), targetName(name), health(hp), maxHealth(hp), scoreValue(score) {}

void TargetComponent::takeDamage(int damage) {
    health = std::max(0, health - damage);
    if (health <= 0) {
        isAlive = false;
        std::cout << "💀 Target " << targetName << " destroyed! Score: " << scoreValue << std::endl;
    } else {
        std::cout << "💔 Target " << targetName << " took " << damage << " damage. Health: " << health << std::endl;
    }
}

void TargetComponent::heal(int amount) {
    health = std::min(maxHealth, health + amount);
    std::cout << "💚 Target " << targetName << " healed " << amount << " health. Health: " << health << std::endl;
}

bool TargetComponent::isInRange(const Vec3& position, float range) const {
    // This would need the transform component to calculate distance
    // For now, return true as a placeholder
    return true;
}

// BuildingComponent implementation
BuildingComponent::BuildingComponent() = default;

BuildingComponent::BuildingComponent(const std::string& type, const std::string& name, int f, float h)
    : buildingType(type), buildingName(name), floors(f), height(h) {}

void BuildingComponent::addEntrance(const Vec3& position) {
    entrancePoints.push_back(position);
    std::cout << "🚪 Added entrance to " << buildingName << " at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

void BuildingComponent::addWindow(const Vec3& position) {
    windowPositions.push_back(position);
    std::cout << "🪟 Added window to " << buildingName << " at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

bool BuildingComponent::canEnter() const {
    return isEnterable && !entrancePoints.empty();
}

Vec3 BuildingComponent::getNearestEntrance(const Vec3& position) const {
    if (entrancePoints.empty()) {
        std::cout << "⚠️ No entrances available in " << buildingName << std::endl;
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

// StreetComponent implementation
StreetComponent::StreetComponent() = default;

StreetComponent::StreetComponent(const std::string& type, const std::string& name, float w, float l)
    : streetType(type), streetName(name), width(w), length(l) {}

void StreetComponent::addWaypoint(const Vec3& position) {
    waypoints.push_back(position);
    std::cout << "📍 Added waypoint to " << streetName << " at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

void StreetComponent::addStreetLight(const Vec3& position) {
    streetLightPositions.push_back(position);
    std::cout << "💡 Added street light to " << streetName << " at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

void StreetComponent::addSidewalk(const Vec3& position) {
    sidewalkPositions.push_back(position);
    std::cout << "🚶 Added sidewalk to " << streetName << " at (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
}

} // namespace Metaverse
