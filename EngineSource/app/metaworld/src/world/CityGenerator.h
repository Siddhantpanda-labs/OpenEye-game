/*
    City Generator - Creates the main city world for the Metaverse Game
    
    This system generates a procedural city using the existing engine systems
    without modifying the core library.
*/
#ifndef CITY_GENERATOR_H
#define CITY_GENERATOR_H

#include <engine/ecs/ecs.h>
#include <engine/core/geometries/geometry.h>
#include <engine/core/materials/physically_based.h>
#include <engine/common.h>
#include <vector>
#include <string>
#include <random>

namespace Metaverse {

// Forward declarations
class BuildingGenerator;
class StreetGenerator;

class CityGenerator {
public:
    CityGenerator();
    ~CityGenerator();

    // Main generation methods
    void generateBasicCity(ECS::World& ecsWorld);
    void generateAdvancedCity(ECS::World& ecsWorld);
    void clearCity(ECS::World& ecsWorld);

    // City configuration
    void setCitySize(int width, int height);
    void setBuildingDensity(float density);
    void setStreetWidth(float width);
    void setBuildingHeightRange(float minHeight, float maxHeight);

    // District generation
    void generateDowntownDistrict(ECS::World& ecsWorld);
    void generateIndustrialDistrict(ECS::World& ecsWorld);
    void generateResidentialDistrict(ECS::World& ecsWorld);
    void generateParkDistrict(ECS::World& ecsWorld);

    // Utility methods
    Vec3 getRandomStreetPosition() const;
    Vec3 getRandomBuildingPosition() const;
    bool isPositionOccupied(const Vec3& position, float radius) const;

    // Statistics
    int getBuildingCount() const;
    int getStreetCount() const;
    float getCityArea() const;

private:
    // City configuration
    int m_cityWidth = 10;
    int m_cityHeight = 10;
    float m_buildingDensity = 0.7f;
    float m_streetWidth = 8.0f;
    float m_minBuildingHeight = 5.0f;
    float m_maxBuildingHeight = 30.0f;
    float m_buildingSpacing = 2.0f;

    // Generation state
    std::vector<Vec3> m_occupiedPositions;
    std::vector<ECS::Entity> m_generatedBuildings;
    std::vector<ECS::Entity> m_generatedStreets;
    std::vector<ECS::Entity> m_generatedProps;

    // Random number generation
    std::mt19937 m_randomGenerator;
    std::uniform_real_distribution<float> m_heightDistribution;
    std::uniform_real_distribution<float> m_positionDistribution;

    // Internal generation methods
    void generateStreetGrid(ECS::World& ecsWorld);
    void generateBuildings(ECS::World& ecsWorld);
    void generateProps(ECS::World& ecsWorld);
    void generateLighting(ECS::World& ecsWorld);
    void generatePlayerSpawn(ECS::World& ecsWorld);

    // Building generation
    ECS::Entity createBuilding(ECS::World& ecsWorld, const Vec3& position, 
                              const std::string& type, float height);
    ECS::Entity createOfficeBuilding(ECS::World& ecsWorld, const Vec3& position, float height);
    ECS::Entity createResidentialBuilding(ECS::World& ecsWorld, const Vec3& position, float height);
    ECS::Entity createIndustrialBuilding(ECS::World& ecsWorld, const Vec3& position, float height);
    ECS::Entity createCommercialBuilding(ECS::World& ecsWorld, const Vec3& position, float height);

    // Street generation
    ECS::Entity createStreet(ECS::World& ecsWorld, const Vec3& start, const Vec3& end, 
                            const std::string& type);
    ECS::Entity createMainStreet(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);
    ECS::Entity createSideStreet(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);
    ECS::Entity createAlley(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);

    // Prop generation
    ECS::Entity createStreetLight(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createTree(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createCar(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createBench(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createTrashCan(ECS::World& ecsWorld, const Vec3& position);

    // Geometry creation helpers
    Core::Geometry* createBuildingGeometry(float width, float height, float depth, int floors);
    Core::Geometry* createStreetGeometry(float width, float length);
    Core::Geometry* createPropGeometry(const std::string& propType);

    // Material creation helpers
    Core::IMaterial* createBuildingMaterial(const std::string& buildingType);
    Core::IMaterial* createStreetMaterial();
    Core::IMaterial* createPropMaterial(const std::string& propType);

    // Utility methods
    Vec3 calculateBuildingPosition(int x, int z) const;
    bool isValidBuildingPosition(const Vec3& position) const;
    std::string getRandomBuildingType() const;
    float getRandomBuildingHeight() const;
    Vec3 getRandomColor() const;

    // City layout helpers
    void createCityBlocks(ECS::World& ecsWorld);
    void createIntersections(ECS::World& ecsWorld);
    void createCityBoundary(ECS::World& ecsWorld);
};

// Building Generator - Specialized building creation
class BuildingGenerator {
public:
    BuildingGenerator();
    ~BuildingGenerator();

    // Building creation
    ECS::Entity createBuilding(ECS::World& ecsWorld, const Vec3& position, 
                              const std::string& type, float height);
    
    // Building types
    ECS::Entity createSkyscraper(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createOfficeTower(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createApartmentBuilding(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createHouse(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createWarehouse(ECS::World& ecsWorld, const Vec3& position);
    ECS::Entity createShop(ECS::World& ecsWorld, const Vec3& position);

private:
    // Building parameters
    struct BuildingParams {
        float width;
        float height;
        float depth;
        int floors;
        std::string materialType;
        Vec3 color;
        bool hasWindows;
        bool hasEntrance;
    };

    BuildingParams getBuildingParams(const std::string& type, float height) const;
    Core::Geometry* createBuildingGeometry(const BuildingParams& params);
    Core::IMaterial* createBuildingMaterial(const BuildingParams& params);
};

// Street Generator - Specialized street creation
class StreetGenerator {
public:
    StreetGenerator();
    ~StreetGenerator();

    // Street creation
    ECS::Entity createStreet(ECS::World& ecsWorld, const Vec3& start, const Vec3& end, 
                            const std::string& type);
    
    // Street types
    ECS::Entity createHighway(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);
    ECS::Entity createMainRoad(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);
    ECS::Entity createSideStreet(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);
    ECS::Entity createAlley(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);
    ECS::Entity createSidewalk(ECS::World& ecsWorld, const Vec3& start, const Vec3& end);

private:
    // Street parameters
    struct StreetParams {
        float width;
        float length;
        std::string materialType;
        Vec3 color;
        bool hasSidewalk;
        bool hasStreetLights;
        bool hasLaneMarkings;
    };

    StreetParams getStreetParams(const std::string& type) const;
    Core::Geometry* createStreetGeometry(const StreetParams& params);
    Core::IMaterial* createStreetMaterial(const StreetParams& params);
};

} // namespace Metaverse

#endif // CITY_GENERATOR_H
