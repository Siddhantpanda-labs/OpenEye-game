#pragma once

#include "api/render/RenderDevice.h"
#include "api/render/TextureCube.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render { class IBL; }
namespace api {

class IBL {
public:
    IBL();
    ~IBL();

    IBL(const IBL&) = delete;
    IBL& operator=(const IBL&) = delete;
    IBL(IBL&& other) noexcept;
    IBL& operator=(IBL&& other) noexcept;

    /** Set device before Initialize (for GPU resource creation). */
    void SetDevice(RenderDevice device);
    bool Initialize(const TextureCube& environment, uint32_t irradianceRes = 32, uint32_t prefilterRes = 512);
    void BindForLighting(uint32_t irradianceUnit = 11, uint32_t prefilterUnit = 12, uint32_t brdfLutUnit = 13) const;

    uint32_t GetIrradianceMap() const;
    uint32_t GetPrefilterMap() const;
    uint32_t GetBRDFLUT() const;
    bool IsValid() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
