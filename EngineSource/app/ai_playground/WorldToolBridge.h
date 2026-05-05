#pragma once

#include "api/world/World.h"

#include <string>
#include <vector>

namespace openeye::ai_playground {

/**
 * Maps chat lines (slash commands for now; LLM tool calls later) to api::world operations only.
 * All writes go through eyelib::api::world::World public methods.
 */
class WorldToolBridge {
public:
    explicit WorldToolBridge(eyelib::api::world::World& world);

    /**
     * Handle one user line. Appends to \a outMessages (assistant / system lines only; caller may prepend "user:").
     * Lines starting with `/` are treated as local commands; other lines get a short hint (LLM in Phase 3).
     */
    void HandleUserLine(const std::string& line, std::vector<std::string>& outMessages);

    /** Compact text summary for tool /query and future LLM context. */
    std::string QueryWorldSummaryText() const;

    /** Bounded JSON snapshot (same as `/observe`) for LLM tools. */
    std::string GetObservationJson() const;

    /**
     * Apply a JSON **array** of plan steps (same schema as `/plan`). Returns compact JSON for tool feedback:
     * `{"ok":true,"steps":N}` or `{"ok":false,"step":i,"error":"..."}`.
     */
    std::string ApplyPlanToolFromStepsJson(const std::string& stepsArrayJsonUtf8);

private:
    eyelib::api::world::World& m_world;

    void AppendHelp(std::vector<std::string>& out) const;
    void RunAIDemo(std::vector<std::string>& outMessages);
    void ExecutePlanFromJsonString(const std::string& json, std::vector<std::string>& outMessages);
    bool ParseSetCommand(const std::string& line, std::string& entityName, std::string& componentName, std::string& jsonOut) const;
    bool ParseGetCommand(const std::string& line, std::string& entityName, std::string& componentName) const;
};

} // namespace openeye::ai_playground
