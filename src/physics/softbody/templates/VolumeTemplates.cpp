#include "VolumeTemplates.h"
#include "physics/softbody/SoftBodyFactory.h"

namespace OpenEye {
namespace SoftBodies {
namespace Templates {

btSoftBody* CreateVolumeSphere(Physics::PhysicsWorld* world, const VolumeSphereConfig& cfg) {
    return Physics::SoftBodyFactory::CreateVolumeSphere(
        world,
        cfg.position,
        cfg.radius,
        cfg.resolution,
        cfg.mass,
        cfg.pressure
    );
}

VolumeSphereConfig CreateDefaultVolumeSphereConfig() {
    VolumeSphereConfig cfg;
    return cfg;
}

} // namespace Templates
} // namespace SoftBodies
} // namespace OpenEye

