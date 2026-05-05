#include "PlaygroundWorldObservation.h"

#include "api/world/World.h"

#include <json.hpp>

namespace openeye::ai_playground {

std::string BuildWorldObservationJson(eyelib::api::world::World& world,
                                      const size_t maxEntities,
                                      const size_t maxCharsPerComponent) {
    const auto entities = world.GetEntities();
    nlohmann::json root;
    root["entity_count"] = entities.size();
    root["schema"] = "openeye.world_observation.v1";

    nlohmann::json arr = nlohmann::json::array();
    size_t listed = 0;
    for (const auto& e : entities) {
        if (listed >= maxEntities) {
            break;
        }
        if (!world.IsAlive(e)) {
            continue;
        }

        nlohmann::json ent;
        ent["index"] = e.index;
        ent["generation"] = e.generation;
        std::string name;
        (void)world.GetEntityName(e, name);
        ent["name"] = name;

        const auto comps = world.GetComponentNames(e);
        ent["components"] = comps;

        nlohmann::json data = nlohmann::json::object();
        for (const auto& c : comps) {
            std::string j;
            if (!world.GetComponentJson(e, c, j)) {
                continue;
            }
            if (j.size() > maxCharsPerComponent) {
                data[c] = nlohmann::json{
                    { "truncated", true },
                    { "preview", j.substr(0, maxCharsPerComponent) }
                };
            } else {
                nlohmann::json parsed = nlohmann::json::parse(j, nullptr, false);
                if (!parsed.is_discarded()) {
                    data[c] = std::move(parsed);
                } else {
                    data[c] = j;
                }
            }
        }
        ent["data"] = std::move(data);
        arr.push_back(std::move(ent));
        ++listed;
    }

    root["entities"] = std::move(arr);
    if (entities.size() > maxEntities) {
        root["truncated"] = true;
        root["truncated_note"] =
            "Some entities omitted; increase maxEntities in BuildWorldObservationJson or use /query for a compact list.";
    }

    return root.dump(2);
}

} // namespace openeye::ai_playground
