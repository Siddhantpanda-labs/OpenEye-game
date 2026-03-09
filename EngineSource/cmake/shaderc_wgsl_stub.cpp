// Stub when WGSL (shaderc_wgsl.cpp) is excluded: Tint not in tree.
// Provides compileWgslShader so shaderc links; WGSL compile will return false.
#include "shaderc.h"

namespace bgfx {

bool compileWgslShader(const Options& /*_options*/, uint32_t /*_version*/,
                      const std::string& /*_code*/, bx::WriterI* /*_writer*/,
                      bx::WriterI* /*_messages*/) {
    return false;
}

}  // namespace bgfx
