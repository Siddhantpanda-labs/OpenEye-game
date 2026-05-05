#pragma once

#include "eyelib/rendering/materials/MaterialSchema.h"

#include <vector>

namespace eyelib {
namespace rendering {
namespace materials {

/**
 * Runtime-owned material table addressed by `DrawItem::materialId`.
 * Slot 0 is always valid and stores the fallback/default material.
 */
class MaterialTable {
public:
    MaterialTable();

    void ResetToDefault();
    uint32_t MaterialCount() const noexcept;

    void EnsureMaterialId(uint32_t materialId);
    bool SetMaterial(uint32_t materialId, const MaterialRecord& record);

    const MaterialRecord& GetMaterialOrDefault(uint32_t materialId) const noexcept;

    void BuildGpuRows(uint32_t requiredCount, std::vector<MaterialGpuStd430>& outRows) const;

private:
    std::vector<MaterialRecord> m_records;
};

} // namespace materials
} // namespace rendering
} // namespace eyelib
