#include "Presets.h"

namespace eyelib {
namespace debug {

Presets& GetPresets() {
    static Presets s_presets{};
    return s_presets;
}

} // namespace debug
} // namespace eyelib

