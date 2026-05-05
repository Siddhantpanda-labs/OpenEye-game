#pragma once

#include "asset/core/import_material.h"

#include <string>

namespace asset {

/** Writes ImportMaterial to canonical .mat binary. Deterministic output. */
class MaterialWriter {
public:
    static bool Write(const ImportMaterial& material, const std::string& path);
};

} // namespace asset
