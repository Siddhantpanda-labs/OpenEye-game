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

static eyelib::fluids::BoundaryMode ToInternal(BoundaryMode m) {
    switch (m) {
    case BoundaryMode::Open: return eyelib::fluids::BoundaryMode::Open;
    case BoundaryMode::Wrap: return eyelib::fluids::BoundaryMode::Wrap;
    default: return eyelib::fluids::BoundaryMode::Solid;
    }
}

static BoundaryMode FromInternal(eyelib::fluids::BoundaryMode m) {
    switch (m) {
    case eyelib::fluids::BoundaryMode::Open: return BoundaryMode::Open;
    case eyelib::fluids::BoundaryMode::Wrap: return BoundaryMode::Wrap;
    default: return BoundaryMode::Solid;
    }
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

static eyelib::fluids::FluidParams ToInternal(const FluidParams& p) {
    eyelib::fluids::FluidParams out;
    out.viscosity = p.viscosity;
    out.diffusion = p.diffusion;
    out.temperatureDiffusion = p.temperatureDiffusion;
    out.densityDissipation = p.densityDissipation;
    out.temperatureDissipation = p.temperatureDissipation;
    out.vorticityStrength = p.vorticityStrength;
    out.buoyancy = p.buoyancy;
    out.weight = p.weight;
    out.ambientTemperature = p.ambientTemperature;
    out.pressureIterations = p.pressureIterations;
    out.diffusionIterations = p.diffusionIterations;
    return out;
}

static FluidParams FromInternal(const eyelib::fluids::FluidParams& p) {
    FluidParams out;
    out.viscosity = p.viscosity;
    out.diffusion = p.diffusion;
    out.temperatureDiffusion = p.temperatureDiffusion;
    out.densityDissipation = p.densityDissipation;
    out.temperatureDissipation = p.temperatureDissipation;
    out.vorticityStrength = p.vorticityStrength;
    out.buoyancy = p.buoyancy;
    out.weight = p.weight;
    out.ambientTemperature = p.ambientTemperature;
    out.pressureIterations = p.pressureIterations;
    out.diffusionIterations = p.diffusionIterations;
    return out;
}

static eyelib::fluids::FluidObstacleBox ToInternal(const FluidObstacleBox& b) {
    eyelib::fluids::FluidObstacleBox out;
    out.center01 = b.center01;
    out.size01 = b.size01;
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

bool FluidSimulation::Initialize(const FluidGridDesc& desc, FluidType type, const FluidParams& params) {
    return m_impl->sim.Initialize(ToInternal(desc), ToInternal(type), ToInternal(params));
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

void FluidSimulation::InjectTemperature(const eyelib::math::Vec3& pos01, float radius01, float amount) {
    m_impl->sim.InjectTemperature(pos01, radius01, amount);
}

void FluidSimulation::InjectVelocity(const eyelib::math::Vec3& pos01, float radius01, const eyelib::math::Vec3& velocity) {
    m_impl->sim.InjectVelocity(pos01, radius01, velocity);
}

void FluidSimulation::ClearDensity() {
    m_impl->sim.ClearDensity();
}

void FluidSimulation::ClearTemperature() {
    m_impl->sim.ClearTemperature();
}

FluidGridDesc FluidSimulation::GetDesc() const {
    return FromInternal(m_impl->sim.GetDesc());
}

FluidType FluidSimulation::GetType() const {
    return FromInternal(m_impl->sim.GetType());
}

FluidParams FluidSimulation::GetParams() const {
    return FromInternal(m_impl->sim.GetParams());
}

void FluidSimulation::SetParams(const FluidParams& params) {
    m_impl->sim.SetParams(ToInternal(params));
}

BoundaryMode FluidSimulation::GetBoundaryMode() const {
    return FromInternal(m_impl->sim.GetBoundaryMode());
}

void FluidSimulation::SetBoundaryMode(BoundaryMode mode) {
    m_impl->sim.SetBoundaryMode(ToInternal(mode));
}

float FluidSimulation::SampleDensity(const eyelib::math::Vec3& pos01) const {
    return m_impl->sim.SampleDensity(pos01);
}

float FluidSimulation::SampleTemperature(const eyelib::math::Vec3& pos01) const {
    return m_impl->sim.SampleTemperature(pos01);
}

eyelib::math::Vec3 FluidSimulation::SampleVelocity(const eyelib::math::Vec3& pos01) const {
    return m_impl->sim.SampleVelocity(pos01);
}

void FluidSimulation::ClearObstacles() {
    m_impl->sim.ClearObstacles();
}

void FluidSimulation::AddObstacleBox(const FluidObstacleBox& box) {
    m_impl->sim.AddObstacleBox(ToInternal(box));
}

bool FluidSimulation::SetObstacleSDF(const std::vector<float>& sdf, float threshold) {
    return m_impl->sim.SetObstacleSDF(sdf, threshold);
}

void FluidSimulation::ClearObstacleSDF() {
    m_impl->sim.ClearObstacleSDF();
}

fluids::FluidSimulation* FluidSimulation::GetInternal() const {
    return &m_impl->sim;
}

} // namespace api
} // namespace eyelib

