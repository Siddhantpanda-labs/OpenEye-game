/*
    City Generator Implementation
*/
#include "CityGenerator.h"
#include "../components/GameComponents.h"
#include <iostream>
#include <algorithm>

namespace Metaverse {

CityGenerator::CityGenerator() 
    : m_randomGenerator(std::random_device{}())
    , m_heightDistribution(m_minBuildingHeight, m_maxBuildingHeight)
    , m_positionDistribution(-50.0f, 50.0f) {
    
    std::cout << "🏙️ City Generator initialized" << std::endl;
}

CityGenerator::~CityGenerator() {
    std::cout << "🏙️ City Generator destroyed" << std::endl;
}

void CityGenerator::generateBasicCity(ECS::World& ecsWorld) {
    std::cout << "🏗️ Generating basic city..." << std::endl;
    
    // Clear any existing city
    clearCity(ecsWorld);
    
    // Generate street grid
    generateStreetGrid(ecsWorld);
    
    // Generate buildings
    generateBuildings(ecsWorld);
    
    // Generate props
    generateProps(ecsWorld);
    
    // Generate lighting
    generateLighting(ecsWorld);
    
    // Generate player spawn
    generatePlayerSpawn(ecsWorld);
    
    std::cout << "✅ Basic city generated successfully!" << std::endl;
    std::cout << "📊 City Statistics:" << std::endl;
    std::cout << "   Buildings: " << getBuildingCount() << std::endl;
    std::cout << "   Streets: " << getStreetCount() << std::endl;
    std::cout << "   Area: " << getCityArea() << " square units" << std::endl;
}

void CityGenerator::generateAdvancedCity(ECS::World& ecsWorld) {
    std::cout << "🏗️ Generating advanced city..." << std::endl;
    
    // Generate districts
    generateDowntownDistrict(ecsWorld);
    generateIndustrialDistrict(ecsWorld);
    generateResidentialDistrict(ecsWorld);
    generateParkDistrict(ecsWorld);
    
    std::cout << "✅ Advanced city generated successfully!" << std::endl;
}

void CityGenerator::clearCity(ECS::World& ecsWorld) {
    std::cout << "🧹 Clearing existing city..." << std::endl;
    
    // Clear generated entities
    for (auto entity : m_generatedBuildings) {
        ecsWorld.destroy_entity(entity);
    }
    for (auto entity : m_generatedStreets) {
        ecsWorld.destroy_entity(entity);
    }
    for (auto entity : m_generatedProps) {
        ecsWorld.destroy_entity(entity);
    }
    
    // Clear tracking vectors
    m_generatedBuildings.clear();
    m_generatedStreets.clear();
    m_generatedProps.clear();
    m_occupiedPositions.clear();
    
    std::cout << "✅ City cleared." << std::endl;
}

void CityGenerator::setCitySize(int width, int height) {
    m_cityWidth = width;
    m_cityHeight = height;
    std::cout << "🏙️ City size set to " << width << "x" << height << std::endl;
}

void CityGenerator::setBuildingDensity(float density) {
    m_buildingDensity = std::clamp(density, 0.0f, 1.0f);
    std::cout << "🏙️ Building density set to " << m_buildingDensity << std::endl;
}

void CityGenerator::setStreetWidth(float width) {
    m_streetWidth = std::max(2.0f, width);
    std::cout << "🏙️ Street width set to " << m_streetWidth << std::endl;
}

void CityGenerator::setBuildingHeightRange(float minHeight, float maxHeight) {
    m_minBuildingHeight = std::max(1.0f, minHeight);
    m_maxBuildingHeight = std::max(m_minBuildingHeight, maxHeight);
    m_heightDistribution = std::uniform_real_distribution<float>(m_minBuildingHeight, m_maxBuildingHeight);
    std::cout << "🏙️ Building height range set to " << m_minBuildingHeight << "-" << m_maxBuildingHeight << std::endl;
}

void CityGenerator::generateDowntownDistrict(ECS::World& ecsWorld) {
    std::cout << "🏢 Generating downtown district..." << std::endl;
    
    // Create tall office buildings in the center
    for (int x = -2; x <= 2; ++x) {
        for (int z = -2; z <= 2; ++z) {
            Vec3 position = Vec3(x * 25.0f, 0.0f, z * 25.0f);
            if (isValidBuildingPosition(position)) {
                float height = 20.0f + (rand() % 20); // 20-40 units tall
                createBuilding(ecsWorld, position, "office", height);
            }
        }
    }
}

void CityGenerator::generateIndustrialDistrict(ECS::World& ecsWorld) {
    std::cout << "🏭 Generating industrial district..." << std::endl;
    
    // Create industrial buildings on the outskirts
    for (int x = -5; x <= -3; ++x) {
        for (int z = -5; z <= 5; ++z) {
            Vec3 position = Vec3(x * 30.0f, 0.0f, z * 30.0f);
            if (isValidBuildingPosition(position)) {
                float height = 8.0f + (rand() % 12); // 8-20 units tall
                createBuilding(ecsWorld, position, "industrial", height);
            }
        }
    }
}

void CityGenerator::generateResidentialDistrict(ECS::World& ecsWorld) {
    std::cout << "🏠 Generating residential district..." << std::endl;
    
    // Create residential buildings
    for (int x = 3; x <= 5; ++x) {
        for (int z = -5; z <= 5; ++z) {
            Vec3 position = Vec3(x * 25.0f, 0.0f, z * 25.0f);
            if (isValidBuildingPosition(position)) {
                float height = 5.0f + (rand() % 10); // 5-15 units tall
                createBuilding(ecsWorld, position, "residential", height);
            }
        }
    }
}

void CityGenerator::generateParkDistrict(ECS::World& ecsWorld) {
    std::cout << "🌳 Generating park district..." << std::endl;
    
    // Create park area with trees and benches
    for (int x = -1; x <= 1; ++x) {
        for (int z = -5; z <= -3; ++z) {
            Vec3 position = Vec3(x * 20.0f, 0.0f, z * 20.0f);
            
            // Add trees
            if (rand() % 3 == 0) {
                createTree(ecsWorld, position);
            }
            
            // Add benches
            if (rand() % 5 == 0) {
                createBench(ecsWorld, position);
            }
        }
    }
}

Vec3 CityGenerator::getRandomStreetPosition() const {
    // Generate random position along streets
    float x = m_positionDistribution(m_randomGenerator);
    float z = m_positionDistribution(m_randomGenerator);
    return Vec3(x, 0.0f, z);
}

Vec3 CityGenerator::getRandomBuildingPosition() const {
    // Generate random position for buildings
    float x = m_positionDistribution(m_randomGenerator);
    float z = m_positionDistribution(m_randomGenerator);
    return Vec3(x, 0.0f, z);
}

bool CityGenerator::isPositionOccupied(const Vec3& position, float radius) const {
    for (const auto& occupiedPos : m_occupiedPositions) {
        if (glm::length(position - occupiedPos) < radius) {
            return true;
        }
    }
    return false;
}

int CityGenerator::getBuildingCount() const {
    return static_cast<int>(m_generatedBuildings.size());
}

int CityGenerator::getStreetCount() const {
    return static_cast<int>(m_generatedStreets.size());
}

float CityGenerator::getCityArea() const {
    return m_cityWidth * m_cityHeight * 100.0f; // Approximate area
}

void CityGenerator::generateStreetGrid(ECS::World& ecsWorld) {
    std::cout << "🛣️ Generating street grid..." << std::endl;
    
    // Create main streets (horizontal and vertical)
    for (int i = -m_cityWidth/2; i <= m_cityWidth/2; ++i) {
        // Horizontal streets
        Vec3 start = Vec3(-m_cityWidth * 25.0f, 0.0f, i * 25.0f);
        Vec3 end = Vec3(m_cityWidth * 25.0f, 0.0f, i * 25.0f);
        createStreet(ecsWorld, start, end, "main");
        
        // Vertical streets
        start = Vec3(i * 25.0f, 0.0f, -m_cityHeight * 25.0f);
        end = Vec3(i * 25.0f, 0.0f, m_cityHeight * 25.0f);
        createStreet(ecsWorld, start, end, "main");
    }
}

void CityGenerator::generateBuildings(ECS::World& ecsWorld) {
    std::cout << "🏢 Generating buildings..." << std::endl;
    
    // Generate buildings in a grid pattern
    for (int x = -m_cityWidth/2; x <= m_cityWidth/2; ++x) {
        for (int z = -m_cityHeight/2; z <= m_cityHeight/2; ++z) {
            // Skip street intersections
            if (x == 0 || z == 0) continue;
            
            Vec3 position = calculateBuildingPosition(x, z);
            
            if (isValidBuildingPosition(position) && (rand() % 100) < (m_buildingDensity * 100)) {
                std::string buildingType = getRandomBuildingType();
                float height = getRandomBuildingHeight();
                
                ECS::Entity building = createBuilding(ecsWorld, position, buildingType, height);
                m_generatedBuildings.push_back(building);
                m_occupiedPositions.push_back(position);
            }
        }
    }
}

void CityGenerator::generateProps(ECS::World& ecsWorld) {
    std::cout << "🌳 Generating props..." << std::endl;
    
    // Add street lights along main streets
    for (int i = -m_cityWidth; i <= m_cityWidth; i += 2) {
        Vec3 position = Vec3(i * 12.5f, 0.0f, 0.0f);
        createStreetLight(ecsWorld, position);
        
        position = Vec3(0.0f, 0.0f, i * 12.5f);
        createStreetLight(ecsWorld, position);
    }
    
    // Add some random trees
    for (int i = 0; i < 20; ++i) {
        Vec3 position = getRandomStreetPosition();
        if (!isPositionOccupied(position, 5.0f)) {
            createTree(ecsWorld, position);
        }
    }
    
    // Add some cars
    for (int i = 0; i < 10; ++i) {
        Vec3 position = getRandomStreetPosition();
        if (!isPositionOccupied(position, 3.0f)) {
            createCar(ecsWorld, position);
        }
    }
}

void CityGenerator::generateLighting(ECS::World& ecsWorld) {
    std::cout << "💡 Generating lighting..." << std::endl;
    
    // Add ambient lighting
    ECS::Entity light = ecsWorld.create_entity();
    ecsWorld.add_component<ECS::TransformComponent>(light, Vec3(0.0f, 50.0f, 0.0f));
    ecsWorld.add_component<ECS::LightComponent>(light, ECS::LightType::Directional, 
        Vec3(1.0f, 0.95f, 0.8f), 1.0f);
    
    // Add some point lights for street lighting
    for (int i = 0; i < 5; ++i) {
        Vec3 position = getRandomStreetPosition();
        position.y = 8.0f; // Street light height
        
        ECS::Entity streetLight = ecsWorld.create_entity();
        ecsWorld.add_component<ECS::TransformComponent>(streetLight, position);
        ecsWorld.add_component<ECS::LightComponent>(streetLight, ECS::LightType::Point, 
            Vec3(1.0f, 0.9f, 0.7f), 0.8f, 20.0f);
    }
}

void CityGenerator::generatePlayerSpawn(ECS::World& ecsWorld) {
    std::cout << "👤 Generating player spawn point..." << std::endl;
    
    // Create a spawn point at the center of the city
    ECS::Entity spawnPoint = ecsWorld.create_entity();
    ecsWorld.add_component<ECS::TransformComponent>(spawnPoint, Vec3(0.0f, 1.0f, 0.0f));
    
    // Add a spawn point marker (invisible but functional)
    // This could be a special component or just a transform
}

ECS::Entity CityGenerator::createBuilding(ECS::World& ecsWorld, const Vec3& position, 
                                        const std::string& type, float height) {
    ECS::Entity building = ecsWorld.create_entity();
    
    // Add transform component
    ecsWorld.add_component<ECS::TransformComponent>(building, position, Vec3(0.0f), Vec3(1.0f));
    
    // Add mesh component (simplified)
    ecsWorld.add_component<ECS::MeshComponent>(building, "building_" + type);
    
    // Add building-specific component
    ecsWorld.add_component<BuildingComponent>(building, type, "Building", 
        static_cast<int>(height / 3.0f), height);
    
    return building;
}

ECS::Entity CityGenerator::createStreet(ECS::World& ecsWorld, const Vec3& start, const Vec3& end, 
                                      const std::string& type) {
    ECS::Entity street = ecsWorld.create_entity();
    
    // Calculate street center and length
    Vec3 center = (start + end) * 0.5f;
    float length = glm::length(end - start);
    
    // Add transform component
    ecsWorld.add_component<ECS::TransformComponent>(street, center, Vec3(0.0f), Vec3(1.0f));
    
    // Add mesh component (simplified)
    ecsWorld.add_component<ECS::MeshComponent>(street, "street_" + type);
    
    // Add street-specific component
    ecsWorld.add_component<StreetComponent>(street, type, "Street", m_streetWidth, length);
    
    m_generatedStreets.push_back(street);
    return street;
}

ECS::Entity CityGenerator::createStreetLight(ECS::World& ecsWorld, const Vec3& position) {
    ECS::Entity light = ecsWorld.create_entity();
    
    // Add transform component
    ecsWorld.add_component<ECS::TransformComponent>(light, position, Vec3(0.0f), Vec3(1.0f));
    
    // Create simple street light geometry (placeholder)
    Core::Geometry* geometry = createPropGeometry("streetlight");
    Core::IMaterial* material = createPropMaterial("streetlight");
    
    // Add mesh component
    ecsWorld.add_component<ECS::MeshComponent>(light, geometry, material);
    
    m_generatedProps.push_back(light);
    return light;
}

ECS::Entity CityGenerator::createTree(ECS::World& ecsWorld, const Vec3& position) {
    ECS::Entity tree = ecsWorld.create_entity();
    
    // Add transform component
    ecsWorld.add_component<ECS::TransformComponent>(tree, position, Vec3(0.0f), Vec3(1.0f));
    
    // Create tree geometry (placeholder)
    Core::Geometry* geometry = createPropGeometry("tree");
    Core::IMaterial* material = createPropMaterial("tree");
    
    // Add mesh component
    ecsWorld.add_component<ECS::MeshComponent>(tree, geometry, material);
    
    m_generatedProps.push_back(tree);
    return tree;
}

ECS::Entity CityGenerator::createCar(ECS::World& ecsWorld, const Vec3& position) {
    ECS::Entity car = ecsWorld.create_entity();
    
    // Add transform component
    ecsWorld.add_component<ECS::TransformComponent>(car, position, Vec3(0.0f), Vec3(1.0f));
    
    // Create car geometry (placeholder)
    Core::Geometry* geometry = createPropGeometry("car");
    Core::IMaterial* material = createPropMaterial("car");
    
    // Add mesh component
    ecsWorld.add_component<ECS::MeshComponent>(car, geometry, material);
    
    m_generatedProps.push_back(car);
    return car;
}

ECS::Entity CityGenerator::createBench(ECS::World& ecsWorld, const Vec3& position) {
    ECS::Entity bench = ecsWorld.create_entity();
    
    // Add transform component
    ecsWorld.add_component<ECS::TransformComponent>(bench, position, Vec3(0.0f), Vec3(1.0f));
    
    // Create bench geometry (placeholder)
    Core::Geometry* geometry = createPropGeometry("bench");
    Core::IMaterial* material = createPropMaterial("bench");
    
    // Add mesh component
    ecsWorld.add_component<ECS::MeshComponent>(bench, geometry, material);
    
    m_generatedProps.push_back(bench);
    return bench;
}

ECS::Entity CityGenerator::createTrashCan(ECS::World& ecsWorld, const Vec3& position) {
    ECS::Entity trashCan = ecsWorld.create_entity();
    
    // Add transform component
    ecsWorld.add_component<ECS::TransformComponent>(trashCan, position, Vec3(0.0f), Vec3(1.0f));
    
    // Create trash can geometry (placeholder)
    Core::Geometry* geometry = createPropGeometry("trashcan");
    Core::IMaterial* material = createPropMaterial("trashcan");
    
    // Add mesh component
    ecsWorld.add_component<ECS::MeshComponent>(trashCan, geometry, material);
    
    m_generatedProps.push_back(trashCan);
    return trashCan;
}

Core::Geometry* CityGenerator::createBuildingGeometry(float width, float height, float depth, int floors) {
    // Create a simple box geometry for buildings
    Core::Geometry* geometry = new Core::Geometry();
    
    // Create vertices for a box
    std::vector<Graphics::Vertex> vertices;
    
    // Front face
    vertices.push_back({{-width/2, 0, depth/2}, {0, 0, 1}, {0, 0}});
    vertices.push_back({{width/2, 0, depth/2}, {0, 0, 1}, {1, 0}});
    vertices.push_back({{width/2, height, depth/2}, {0, 0, 1}, {1, 1}});
    vertices.push_back({{-width/2, height, depth/2}, {0, 0, 1}, {0, 1}});
    
    // Back face
    vertices.push_back({{width/2, 0, -depth/2}, {0, 0, -1}, {0, 0}});
    vertices.push_back({{-width/2, 0, -depth/2}, {0, 0, -1}, {1, 0}});
    vertices.push_back({{-width/2, height, -depth/2}, {0, 0, -1}, {1, 1}});
    vertices.push_back({{width/2, height, -depth/2}, {0, 0, -1}, {0, 1}});
    
    // Left face
    vertices.push_back({{-width/2, 0, -depth/2}, {-1, 0, 0}, {0, 0}});
    vertices.push_back({{-width/2, 0, depth/2}, {-1, 0, 0}, {1, 0}});
    vertices.push_back({{-width/2, height, depth/2}, {-1, 0, 0}, {1, 1}});
    vertices.push_back({{-width/2, height, -depth/2}, {-1, 0, 0}, {0, 1}});
    
    // Right face
    vertices.push_back({{width/2, 0, depth/2}, {1, 0, 0}, {0, 0}});
    vertices.push_back({{width/2, 0, -depth/2}, {1, 0, 0}, {1, 0}});
    vertices.push_back({{width/2, height, -depth/2}, {1, 0, 0}, {1, 1}});
    vertices.push_back({{width/2, height, depth/2}, {1, 0, 0}, {0, 1}});
    
    // Top face
    vertices.push_back({{-width/2, height, depth/2}, {0, 1, 0}, {0, 0}});
    vertices.push_back({{width/2, height, depth/2}, {0, 1, 0}, {1, 0}});
    vertices.push_back({{width/2, height, -depth/2}, {0, 1, 0}, {1, 1}});
    vertices.push_back({{-width/2, height, -depth/2}, {0, 1, 0}, {0, 1}});
    
    // Bottom face
    vertices.push_back({{-width/2, 0, -depth/2}, {0, -1, 0}, {0, 0}});
    vertices.push_back({{width/2, 0, -depth/2}, {0, -1, 0}, {1, 0}});
    vertices.push_back({{width/2, 0, depth/2}, {0, -1, 0}, {1, 1}});
    vertices.push_back({{-width/2, 0, depth/2}, {0, -1, 0}, {0, 1}});
    
    // Create indices for the box
    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,       // Front
        4, 5, 6, 6, 7, 4,       // Back
        8, 9, 10, 10, 11, 8,    // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };
    
    geometry->fill(vertices, indices);
    return geometry;
}

Core::Geometry* CityGenerator::createStreetGeometry(float width, float length) {
    // Create a simple plane geometry for streets
    Core::Geometry* geometry = new Core::Geometry();
    
    std::vector<Graphics::Vertex> vertices;
    
    // Create a plane
    vertices.push_back({{-length/2, 0, -width/2}, {0, 1, 0}, {0, 0}});
    vertices.push_back({{length/2, 0, -width/2}, {0, 1, 0}, {1, 0}});
    vertices.push_back({{length/2, 0, width/2}, {0, 1, 0}, {1, 1}});
    vertices.push_back({{-length/2, 0, width/2}, {0, 1, 0}, {0, 1}});
    
    std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
    
    geometry->fill(vertices, indices);
    return geometry;
}

Core::Geometry* CityGenerator::createPropGeometry(const std::string& propType) {
    // Create simple placeholder geometry for props
    Core::Geometry* geometry = new Core::Geometry();
    
    if (propType == "tree") {
        // Simple tree (cylinder + sphere)
        std::vector<Graphics::Vertex> vertices;
        vertices.push_back({{0, 0, 0}, {0, 1, 0}, {0.5, 0.5}});
        vertices.push_back({{0, 2, 0}, {0, 1, 0}, {0.5, 0.5}});
        geometry->fill(vertices);
    } else if (propType == "car") {
        // Simple car (box)
        std::vector<Graphics::Vertex> vertices;
        vertices.push_back({{-2, 0, -1}, {0, 1, 0}, {0, 0}});
        vertices.push_back({{2, 0, -1}, {0, 1, 0}, {1, 0}});
        vertices.push_back({{2, 1, 1}, {0, 1, 0}, {1, 1}});
        vertices.push_back({{-2, 1, 1}, {0, 1, 0}, {0, 1}});
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        geometry->fill(vertices, indices);
    } else {
        // Default box
        std::vector<Graphics::Vertex> vertices;
        vertices.push_back({{-0.5, 0, -0.5}, {0, 1, 0}, {0, 0}});
        vertices.push_back({{0.5, 0, -0.5}, {0, 1, 0}, {1, 0}});
        vertices.push_back({{0.5, 1, 0.5}, {0, 1, 0}, {1, 1}});
        vertices.push_back({{-0.5, 1, 0.5}, {0, 1, 0}, {0, 1}});
        std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};
        geometry->fill(vertices, indices);
    }
    
    return geometry;
}

Core::IMaterial* CityGenerator::createBuildingMaterial(const std::string& buildingType) {
    Core::PhysicalMaterial* material = new Core::PhysicalMaterial("pbr_shader");
    
    if (buildingType == "office") {
        material->set_albedo(Vec4(0.8f, 0.8f, 0.9f, 1.0f));
        material->set_metalness(0.1f);
        material->set_roughness(0.3f);
    } else if (buildingType == "residential") {
        material->set_albedo(Vec4(0.9f, 0.8f, 0.7f, 1.0f));
        material->set_metalness(0.0f);
        material->set_roughness(0.5f);
    } else if (buildingType == "industrial") {
        material->set_albedo(Vec4(0.6f, 0.6f, 0.6f, 1.0f));
        material->set_metalness(0.2f);
        material->set_roughness(0.7f);
    } else {
        material->set_albedo(Vec4(0.7f, 0.7f, 0.7f, 1.0f));
        material->set_metalness(0.1f);
        material->set_roughness(0.4f);
    }
    
    return material;
}

Core::IMaterial* CityGenerator::createStreetMaterial() {
    Core::PhysicalMaterial* material = new Core::PhysicalMaterial("pbr_shader");
    material->set_albedo(Vec4(0.3f, 0.3f, 0.3f, 1.0f));
    material->set_metalness(0.0f);
    material->set_roughness(0.8f);
    return material;
}

Core::IMaterial* CityGenerator::createPropMaterial(const std::string& propType) {
    Core::PhysicalMaterial* material = new Core::PhysicalMaterial("pbr_shader");
    
    if (propType == "tree") {
        material->set_albedo(Vec4(0.2f, 0.6f, 0.2f, 1.0f));
        material->set_metalness(0.0f);
        material->set_roughness(0.9f);
    } else if (propType == "car") {
        material->set_albedo(Vec4(0.8f, 0.2f, 0.2f, 1.0f));
        material->set_metalness(0.8f);
        material->set_roughness(0.1f);
    } else {
        material->set_albedo(Vec4(0.5f, 0.5f, 0.5f, 1.0f));
        material->set_metalness(0.1f);
        material->set_roughness(0.5f);
    }
    
    return material;
}

Vec3 CityGenerator::calculateBuildingPosition(int x, int z) const {
    return Vec3(x * 25.0f, 0.0f, z * 25.0f);
}

bool CityGenerator::isValidBuildingPosition(const Vec3& position) const {
    // Check if position is not too close to streets or other buildings
    return !isPositionOccupied(position, 15.0f);
}

std::string CityGenerator::getRandomBuildingType() const {
    std::vector<std::string> types = {"office", "residential", "commercial", "industrial"};
    return types[rand() % types.size()];
}

float CityGenerator::getRandomBuildingHeight() const {
    return m_heightDistribution(m_randomGenerator);
}

Vec3 CityGenerator::getRandomColor() const {
    return Vec3(
        static_cast<float>(rand()) / RAND_MAX,
        static_cast<float>(rand()) / RAND_MAX,
        static_cast<float>(rand()) / RAND_MAX
    );
}

} // namespace Metaverse
