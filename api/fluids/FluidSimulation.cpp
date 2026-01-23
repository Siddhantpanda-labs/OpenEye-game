#include "FluidSimulation.h"

#include "eyelib/fluids/FluidSimulation.h"

namespace eyelib {
namespace api {

namespace {
static eyelib::fluids::FluidType ToInternal(FluidType t) {
    return (t == FluidType::Liquid) ? eyelib::fluids::FluidType::Liquid : eyelib::fluids::FluidType::Gas;
}

static FluidType FromInternal(eyelib::fluids::FluidType t) {
    return (t == eyelib::fluids::FluidType::Liquid) ? FluidType::Liquid : FluidType::Gas;
}

static eyelib::fluids::FluidGridDesc ToInternal(const FluidGridDesc& d) {
    eyelib::fluids::FluidGridDesc out;
    out.resolution = d.resolution;
    out.volumeSize = d.volumeSize;
    return out;
}

static FluidGridDesc FromInternal(const eyelib::fluids::FluidGridDesc& d) {
    FluidGridDesc out;
    out.resolution = d.resolution;
    out.volumeSize = d.volumeSize;
    return out;
}
} // namespace

class FluidSimulation::Impl {
public:
    eyelib::fluids::FluidSimulation sim{};
};

FluidSimulation::FluidSimulation() : m_impl(new Impl()) {}

FluidSimulation::~FluidSimulation() {
    delete m_impl;
    m_impl = nullptr;
}

bool FluidSimulation::Initialize(const FluidGridDesc& desc, FluidType type) {
    return m_impl->sim.Initialize(ToInternal(desc), ToInternal(type));
}

void FluidSimulation::Shutdown() {
    m_impl->sim.Shutdown();
}

bool FluidSimulation::IsInitialized() const {
    return m_impl->sim.IsInitialized();
}

void FluidSimulation::Step(float dt) {
    m_impl->sim.Step(dt);
}

void FluidSimulation::InjectDensity(const eyelib::math::Vec3& pos01, float radius01, float amount) {
    m_impl->sim.InjectDensity(pos01, radius01, amount);
}

void FluidSimulation::InjectVelocity(const eyelib::math::Vec3& pos01, float radius01, const eyelib::math::Vec3& velocity) {
    m_impl->sim.InjectVelocity(pos01, radius01, velocity);
}

void FluidSimulation::ClearDensity() {
    m_impl->sim.ClearDensity();
}

FluidGridDesc FluidSimulation::GetDesc() const {
    return FromInternal(m_impl->sim.GetDesc());
}

FluidType FluidSimulation::GetType() const {
    return FromInternal(m_impl->sim.GetType());
}

float FluidSimulation::SampleDensity(const eyelib::math::Vec3& pos01) const {
    return m_impl->sim.SampleDensity(pos01);
}

} // namespace api
} // namespace eyelib

