#pragma once

#include "eyelib/math/Types.h"
#include <memory>
#include <functional>

namespace eyelib {
namespace geometry {

/**
 * @brief ScalarField: Base class for scalar fields (float per position).
 * 
 * Fields are mathematical abstractions that can be evaluated at any point in space.
 * Used for procedural sculpting, painting, and deformation.
 */
class ScalarField {
public:
    virtual ~ScalarField() = default;
    
    /**
     * @brief Sample the field at a given position.
     * @param position World-space position
     * @return Scalar value at that position
     */
    virtual float Sample(const math::Vec3& position) const = 0;
    
    /**
     * @brief Create a constant field.
     */
    static std::shared_ptr<ScalarField> Constant(float value);
    
    /**
     * @brief Create a field from a function.
     */
    static std::shared_ptr<ScalarField> FromFunction(std::function<float(const math::Vec3&)> func);
};

/**
 * @brief VectorField: Base class for vector fields (vec3 per position).
 * 
 * Used for directional deformations and vector-based operations.
 */
class VectorField {
public:
    virtual ~VectorField() = default;
    
    /**
     * @brief Sample the field at a given position.
     * @param position World-space position
     * @return Vector value at that position
     */
    virtual math::Vec3 Sample(const math::Vec3& position) const = 0;
    
    /**
     * @brief Create a constant vector field.
     */
    static std::shared_ptr<VectorField> Constant(const math::Vec3& value);
    
    /**
     * @brief Create a field from a function.
     */
    static std::shared_ptr<VectorField> FromFunction(std::function<math::Vec3(const math::Vec3&)> func);
};

/**
 * @brief DistanceField: Scalar field representing distance from a point/surface.
 */
class DistanceField : public ScalarField {
public:
    /**
     * @brief Create distance field from a point.
     * @param center Center point
     * @param maxDistance Maximum distance (beyond this, returns maxDistance)
     */
    static std::shared_ptr<DistanceField> FromPoint(const math::Vec3& center, float maxDistance = 1000.0f);
    
    /**
     * @brief Create distance field from a sphere.
     * @param center Sphere center
     * @param radius Sphere radius
     */
    static std::shared_ptr<DistanceField> FromSphere(const math::Vec3& center, float radius);
    
    float Sample(const math::Vec3& position) const override;
    
    void SetCenter(const math::Vec3& center) { m_center = center; }
    void SetRadius(float radius) { m_radius = radius; }
    void SetMaxDistance(float maxDist) { m_maxDistance = maxDist; }
    
    // Type enum (public for factory methods)
    enum class Type {
        Point,
        Sphere
    };
    
    // Constructor (public for std::make_shared, but use factory methods FromPoint/FromSphere)
    DistanceField(Type type, const math::Vec3& center, float radius, float maxDistance)
        : m_type(type), m_center(center), m_radius(radius), m_maxDistance(maxDistance) {}
    
private:
    Type m_type = Type::Point;
    math::Vec3 m_center{0, 0, 0};
    float m_radius = 1.0f;
    float m_maxDistance = 1000.0f;
};

/**
 * @brief Field operations: combine and manipulate fields.
 */
class FieldOps {
public:
    /**
     * @brief Add two scalar fields.
     */
    static std::shared_ptr<ScalarField> Add(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b);
    
    /**
     * @brief Multiply two scalar fields.
     */
    static std::shared_ptr<ScalarField> Multiply(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b);
    
    /**
     * @brief Blend two scalar fields.
     * @param a First field
     * @param b Second field
     * @param blendFactor Blend factor (0 = all a, 1 = all b)
     */
    static std::shared_ptr<ScalarField> Blend(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b, float blendFactor);
    
    /**
     * @brief Scale a scalar field.
     */
    static std::shared_ptr<ScalarField> Scale(std::shared_ptr<const ScalarField> field, float scale);
    
    /**
     * @brief Add two vector fields.
     */
    static std::shared_ptr<VectorField> Add(std::shared_ptr<const VectorField> a, std::shared_ptr<const VectorField> b);
    
    /**
     * @brief Scale a vector field.
     */
    static std::shared_ptr<VectorField> Scale(std::shared_ptr<const VectorField> field, float scale);
};

} // namespace geometry
} // namespace eyelib
