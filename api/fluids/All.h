#pragma once

#include "FluidSimulation.h"
#include "FluidTemplates.h"
#include "FluidRenderer.h"
#include "FluidFoamRenderer.h"
#include "FluidSurfaceExtractor.h"
#include "FluidVolumeRenderer.h"

namespace eyelib {
namespace api {
namespace fluids {
using ::eyelib::api::FluidSimulation;
using ::eyelib::api::FluidRenderer;
using ::eyelib::api::FluidFoamRenderer;
using ::eyelib::api::FluidSurfaceExtractor;
using ::eyelib::api::FluidVolumeRenderer;
using ::eyelib::api::FluidGridDesc;
using ::eyelib::api::FluidParams;
using ::eyelib::api::FluidType;
using ::eyelib::api::BoundaryMode;
using ::eyelib::api::FluidObstacleBox;
using ::eyelib::api::FluidTemplates;
} // namespace fluids
} // namespace api
} // namespace eyelib

