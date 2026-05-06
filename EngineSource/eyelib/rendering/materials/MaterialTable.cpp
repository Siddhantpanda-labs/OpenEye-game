#include "eyelib/rendering/materials/MaterialTable.h"

#include <cstring>

namespace eyelib {
namespace rendering {
namespace materials {

MaterialTable::MaterialTable() {
    ResetToDefault();
}

void MaterialTable::ResetToDefault() {
    m_records.clear();
    m_records.push_back(MakeDefaultMaterialRecord());
    ++m_revision;
}

uint32_t MaterialTable::MaterialCount() const noexcept {
    return static_cast<uint32_t>(m_records.size());
}

void MaterialTable::EnsureMaterialId(uint32_t materialId) {
    if (materialId < m_records.size())
        return;
    m_records.resize(static_cast<size_t>(materialId) + 1u, MakeDefaultMaterialRecord());
    ++m_revision;
}

bool MaterialTable::SetMaterial(uint32_t materialId, const MaterialRecord& record) {
    EnsureMaterialId(materialId);
    if (materialId >= m_records.size())
        return false;
    if (std::memcmp(&m_records[materialId], &record, sizeof(MaterialRecord)) == 0)
        return true;
    m_records[materialId] = record;
    ++m_revision;
    return true;
}

const MaterialRecord& MaterialTable::GetMaterialOrDefault(uint32_t materialId) const noexcept {
    if (materialId < m_records.size())
        return m_records[materialId];
    return m_records[0];
}

void MaterialTable::BuildGpuRows(uint32_t requiredCount, std::vector<MaterialGpuStd430>& outRows) const {
    const uint32_t count = (requiredCount == 0u) ? 1u : requiredCount;
    outRows.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        const MaterialRecord& src = GetMaterialOrDefault(i);
        FillMaterialGpuRow(src, outRows[i]);
    }
}

} // namespace materials
} // namespace rendering
} // namespace eyelib
