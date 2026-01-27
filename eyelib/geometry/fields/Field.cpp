#include "eyelib/geometry/fields/Field.h"
#include <cmath>

namespace eyelib {
namespace geometry {

// ScalarField implementation

class ConstantScalarField : public ScalarField {
public:
    explicit ConstantScalarField(float value) : m_value(value) {}
    float Sample(const math::Vec3& position) const override { (void)position; return m_value; }
private:
    float m_value;
};

class FunctionScalarField : public ScalarField {
public:
    explicit FunctionScalarField(std::function<float(const math::Vec3&)> func) : m_func(std::move(func)) {}
    float Sample(const math::Vec3& position) const override { return m_func(position); }
private:
    std::function<float(const math::Vec3&)> m_func;
};

std::shared_ptr<ScalarField> ScalarField::Constant(float value) {
    return std::make_shared<ConstantScalarField>(value);
}

std::shared_ptr<ScalarField> ScalarField::FromFunction(std::function<float(const math::Vec3&)> func) {
    return std::make_shared<FunctionScalarField>(std::move(func));
}

// VectorField implementation

class ConstantVectorField : public VectorField {
public:
    explicit ConstantVectorField(const math::Vec3& value) : m_value(value) {}
    math::Vec3 Sample(const math::Vec3& position) const override { (void)position; return m_value; }
private:
    math::Vec3 m_value;
};

class FunctionVectorField : public VectorField {
public:
    explicit FunctionVectorField(std::function<math::Vec3(const math::Vec3&)> func) : m_func(std::move(func)) {}
    math::Vec3 Sample(const math::Vec3& position) const override { return m_func(position); }
private:
    std::function<math::Vec3(const math::Vec3&)> m_func;
};

std::shared_ptr<VectorField> VectorField::Constant(const math::Vec3& value) {
    return std::make_shared<ConstantVectorField>(value);
}

std::shared_ptr<VectorField> VectorField::FromFunction(std::function<math::Vec3(const math::Vec3&)> func) {
    return std::make_shared<FunctionVectorField>(std::move(func));
}

// DistanceField implementation
// Constructor is now inline in header

std::shared_ptr<DistanceField> DistanceField::FromPoint(const math::Vec3& center, float maxDistance) {
    return std::make_shared<DistanceField>(Type::Point, center, 0.0f, maxDistance);
}

std::shared_ptr<DistanceField> DistanceField::FromSphere(const math::Vec3& center, float radius) {
    return std::make_shared<DistanceField>(Type::Sphere, center, radius, radius * 10.0f);
}

float DistanceField::Sample(const math::Vec3& position) const {
    math::Vec3 diff = position - m_center;
    float distance = glm::length(diff);
    
    if (m_type == Type::Point) {
        return std::min(distance, m_maxDistance);
    } else { // Sphere
        float distToSurface = std::abs(distance - m_radius);
        return std::min(distToSurface, m_maxDistance);
    }
}

// FieldOps implementation

class AddScalarField : public ScalarField {
public:
    AddScalarField(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b)
        : m_a(a), m_b(b) {}
    float Sample(const math::Vec3& position) const override {
        return m_a->Sample(position) + m_b->Sample(position);
    }
private:
    std::shared_ptr<const ScalarField> m_a, m_b;
};

class MultiplyScalarField : public ScalarField {
public:
    MultiplyScalarField(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b)
        : m_a(a), m_b(b) {}
    float Sample(const math::Vec3& position) const override {
        return m_a->Sample(position) * m_b->Sample(position);
    }
private:
    std::shared_ptr<const ScalarField> m_a, m_b;
};

class BlendScalarField : public ScalarField {
public:
    BlendScalarField(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b, float blend)
        : m_a(a), m_b(b), m_blend(blend) {}
    float Sample(const math::Vec3& position) const override {
        float va = m_a->Sample(position);
        float vb = m_b->Sample(position);
        return va * (1.0f - m_blend) + vb * m_blend;
    }
private:
    std::shared_ptr<const ScalarField> m_a, m_b;
    float m_blend;
};

class ScaleScalarField : public ScalarField {
public:
    ScaleScalarField(std::shared_ptr<const ScalarField> field, float scale)
        : m_field(field), m_scale(scale) {}
    float Sample(const math::Vec3& position) const override {
        return m_field->Sample(position) * m_scale;
    }
private:
    std::shared_ptr<const ScalarField> m_field;
    float m_scale;
};

std::shared_ptr<ScalarField> FieldOps::Add(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b) {
    return std::make_shared<AddScalarField>(a, b);
}

std::shared_ptr<ScalarField> FieldOps::Multiply(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b) {
    return std::make_shared<MultiplyScalarField>(a, b);
}

std::shared_ptr<ScalarField> FieldOps::Blend(std::shared_ptr<const ScalarField> a, std::shared_ptr<const ScalarField> b, float blendFactor) {
    return std::make_shared<BlendScalarField>(a, b, blendFactor);
}

std::shared_ptr<ScalarField> FieldOps::Scale(std::shared_ptr<const ScalarField> field, float scale) {
    return std::make_shared<ScaleScalarField>(field, scale);
}

class AddVectorField : public VectorField {
public:
    AddVectorField(std::shared_ptr<const VectorField> a, std::shared_ptr<const VectorField> b)
        : m_a(a), m_b(b) {}
    math::Vec3 Sample(const math::Vec3& position) const override {
        return m_a->Sample(position) + m_b->Sample(position);
    }
private:
    std::shared_ptr<const VectorField> m_a, m_b;
};

class ScaleVectorField : public VectorField {
public:
    ScaleVectorField(std::shared_ptr<const VectorField> field, float scale)
        : m_field(field), m_scale(scale) {}
    math::Vec3 Sample(const math::Vec3& position) const override {
        return m_field->Sample(position) * m_scale;
    }
private:
    std::shared_ptr<const VectorField> m_field;
    float m_scale;
};

std::shared_ptr<VectorField> FieldOps::Add(std::shared_ptr<const VectorField> a, std::shared_ptr<const VectorField> b) {
    return std::make_shared<AddVectorField>(a, b);
}

std::shared_ptr<VectorField> FieldOps::Scale(std::shared_ptr<const VectorField> field, float scale) {
    return std::make_shared<ScaleVectorField>(field, scale);
}

} // namespace geometry
} // namespace eyelib
