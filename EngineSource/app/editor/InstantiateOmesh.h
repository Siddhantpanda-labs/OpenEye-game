#pragma once

#include "api/world/All.h"

#include <string>

namespace openeye::editor {

/**
 * Instantiate an .omesh model into the world: create a root entity and child entities from the
 * prefab hierarchy (name, local transform, submesh index). Each mesh node gets the same
 * meshAssetId (GUID) and its submeshIndex. Returns the root entity id or InvalidEntity on failure.
 * \param world        World to create entities in.
 * \param omeshPath     Full path to the .omesh file (used to read prefab from package).
 * \param meshAssetId   GUID or path to use as meshAssetId for all submesh renderables (same for whole model).
 * \param parent        Optional parent entity; use InvalidEntity to add under scene root.
 * \return Root entity of the instantiated hierarchy, or InvalidEntity on error.
 */
::eyelib::world::EntityId InstantiateOmesh(
    eyelib::api::world::World& world,
    const std::string& omeshPath,
    const std::string& meshAssetId,
    ::eyelib::world::EntityId parent = ::eyelib::world::InvalidEntity);

/** Read GUID from asset path (reads path + ".meta", returns guid line value). Returns empty if not found. */
std::string GetGuidFromMetaPath(const std::string& assetPath);

} // namespace openeye::editor
