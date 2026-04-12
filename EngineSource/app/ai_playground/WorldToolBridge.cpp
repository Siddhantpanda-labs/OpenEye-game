#include "WorldToolBridge.h"

#include "PlaygroundWorldObservation.h"

#include <cctype>
#include <sstream>

#include <json.hpp>

namespace openeye::ai_playground {

namespace {

std::string Trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string FirstToken(std::string& rest) {
    rest = Trim(rest);
    if (rest.empty()) {
        return {};
    }
    const auto sp = rest.find(' ');
    if (sp == std::string::npos) {
        std::string t = rest;
        rest.clear();
        return t;
    }
    std::string t = rest.substr(0, sp);
    rest = Trim(rest.substr(sp + 1));
    return t;
}

void DestroyByName(eyelib::api::world::World& world, const std::string& name) {
    const auto id = world.FindEntityByName(name);
    if (world.IsAlive(id)) {
        (void)world.DestroyEntity(id);
    }
}

std::string JsonTransform(const nlohmann::json& position, const nlohmann::json& rotation, const nlohmann::json& scale) {
    nlohmann::json j;
    j["position"] = position;
    j["rotation"] = rotation;
    j["scale"] = scale;
    return j.dump();
}

bool ExecutePlanStep(eyelib::api::world::World& world, const nlohmann::json& step, std::string& err) {
    if (!step.is_object()) {
        err = "step is not an object";
        return false;
    }
    if (!step.contains("op") || !step["op"].is_string()) {
        err = "step missing string op";
        return false;
    }
    const std::string op = step["op"].get<std::string>();

    if (op == "create") {
        if (!step.contains("name") || !step["name"].is_string()) {
            err = "create: need string name";
            return false;
        }
        const std::string name = step["name"].get<std::string>();
        const auto id = world.CreateEntity(name);
        if (!world.IsAlive(id)) {
            err = "CreateEntity failed for \"" + name + "\"";
            return false;
        }
        return true;
    }

    if (op == "destroy") {
        if (!step.contains("name") || !step["name"].is_string()) {
            err = "destroy: need string name";
            return false;
        }
        const std::string name = step["name"].get<std::string>();
        const auto id = world.FindEntityByName(name);
        if (!world.IsAlive(id)) {
            err = "destroy: no entity \"" + name + "\"";
            return false;
        }
        if (!world.DestroyEntity(id)) {
            err = "DestroyEntity failed for \"" + name + "\"";
            return false;
        }
        return true;
    }

    if (op == "set") {
        if (!step.contains("name") || !step["name"].is_string()) {
            err = "set: need string name";
            return false;
        }
        if (!step.contains("component") || !step["component"].is_string()) {
            err = "set: need string component";
            return false;
        }
        if (!step.contains("json")) {
            err = "set: need json (object or value)";
            return false;
        }
        const std::string name = step["name"].get<std::string>();
        const std::string component = step["component"].get<std::string>();
        const auto id = world.FindEntityByName(name);
        if (!world.IsAlive(id)) {
            err = "set: no entity \"" + name + "\"";
            return false;
        }
        std::string jsonStr;
        if (step["json"].is_string()) {
            jsonStr = step["json"].get<std::string>();
        } else {
            jsonStr = step["json"].dump();
        }
        if (!world.SetComponentJson(id, component, jsonStr)) {
            err = "SetComponentJson failed on \"" + name + "\"." + component;
            return false;
        }
        return true;
    }

    err = "unknown op \"" + op + "\" (use create, set, destroy)";
    return false;
}

} // namespace

WorldToolBridge::WorldToolBridge(eyelib::api::world::World& world) : m_world(world) {}

std::string WorldToolBridge::QueryWorldSummaryText() const {
    std::ostringstream oss;
    const auto entities = m_world.GetEntities();
    oss << "entities: " << entities.size() << "\n";
    for (const auto& e : entities) {
        std::string name;
        (void)m_world.GetEntityName(e, name);
        oss << "- id=" << static_cast<unsigned>(e.index) << " gen=" << static_cast<unsigned>(e.generation)
            << " name=\"" << name << "\" components:";
        const auto comps = m_world.GetComponentNames(e);
        for (const auto& c : comps) {
            oss << " " << c;
        }
        oss << "\n";
    }
    return oss.str();
}

std::string WorldToolBridge::GetObservationJson() const {
    return BuildWorldObservationJson(m_world);
}

std::string WorldToolBridge::ApplyPlanToolFromStepsJson(const std::string& stepsArrayJsonUtf8) {
    const nlohmann::json root = nlohmann::json::parse(stepsArrayJsonUtf8, nullptr, false);
    if (root.is_discarded() || !root.is_array()) {
        return R"({"ok":false,"error":"expected_json_array_of_steps"})";
    }
    const size_t n = root.size();
    for (size_t i = 0; i < n; ++i) {
        std::string err;
        if (!ExecutePlanStep(m_world, root[i], err)) {
            m_world.FlushTransforms();
            nlohmann::json out{ { "ok", false }, { "step", i }, { "error", err } };
            return out.dump();
        }
    }
    m_world.FlushTransforms();
    return nlohmann::json{ { "ok", true }, { "steps", n } }.dump();
}

void WorldToolBridge::AppendHelp(std::vector<std::string>& out) const {
    out.push_back("system: Phase 1 — world commands + OpenAI `/ask`:");
    out.push_back("  /help — this text");
    out.push_back("  /ask <goal> — OpenAI tool loop (needs OPENAI_API_KEY; optional OPENAI_MODEL)");
    out.push_back("  /query — compact entity + component name list");
    out.push_back("  /observe — JSON snapshot (names + component data) for model context");
    out.push_back("  /plan <json-array> — run a plan: [{\"op\":\"create\",\"name\":\"…\"}, {\"op\":\"set\",…}, …]");
    out.push_back("  /ai demo — repeatable demo: plane + cube + directional light (AI_PG_*)");
    out.push_back("  /create <name> — CreateEntity");
    out.push_back("  /destroy <name> — DestroyEntity by metadata name");
    out.push_back("  /get <name> <Component> — print component JSON");
    out.push_back("  /set <name> <Component> <json> — SetComponentJson");
    out.push_back("Tool catalog (for prompts): documentation/Ai_plan/tools_catalog_v1.md");
}

bool WorldToolBridge::ParseSetCommand(const std::string& line, std::string& entityName, std::string& componentName,
                                      std::string& jsonOut) const {
    const std::string prefix = "/set ";
    if (line.size() < prefix.size() || line.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    std::string rest = line.substr(prefix.size());
    entityName = FirstToken(rest);
    componentName = FirstToken(rest);
    jsonOut = Trim(rest);
    return !entityName.empty() && !componentName.empty() && !jsonOut.empty();
}

bool WorldToolBridge::ParseGetCommand(const std::string& line, std::string& entityName, std::string& componentName) const {
    const std::string prefix = "/get ";
    if (line.size() < prefix.size() || line.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    std::string rest = line.substr(prefix.size());
    entityName = FirstToken(rest);
    componentName = FirstToken(rest);
    return !entityName.empty() && !componentName.empty() && Trim(rest).empty();
}

void WorldToolBridge::ExecutePlanFromJsonString(const std::string& json, std::vector<std::string>& outMessages) {
    nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
    if (root.is_discarded()) {
        outMessages.push_back("assistant: /plan — invalid JSON.");
        return;
    }
    if (!root.is_array()) {
        outMessages.push_back("assistant: /plan — top level must be a JSON array of steps.");
        return;
    }

    const size_t n = root.size();
    for (size_t i = 0; i < n; ++i) {
        std::string err;
        if (!ExecutePlanStep(m_world, root[i], err)) {
            outMessages.push_back("assistant: /plan step " + std::to_string(i + 1) + "/" + std::to_string(n) + " FAILED: " + err);
            m_world.FlushTransforms();
            return;
        }
        outMessages.push_back("assistant: /plan step " + std::to_string(i + 1) + "/" + std::to_string(n) + " OK.");
    }
    m_world.FlushTransforms();
    outMessages.push_back("assistant: /plan complete (" + std::to_string(n) + " steps).");
}

void WorldToolBridge::RunAIDemo(std::vector<std::string>& outMessages) {
    constexpr const char* kGround = "AI_PG_Ground";
    constexpr const char* kCrate = "AI_PG_Crate";
    constexpr const char* kSun = "AI_PG_Sun";

    DestroyByName(m_world, kGround);
    DestroyByName(m_world, kCrate);
    DestroyByName(m_world, kSun);

    auto fail = [&](const char* msg) {
        outMessages.push_back(std::string("assistant: /ai demo failed: ") + msg);
        m_world.FlushTransforms();
    };

    const auto groundId = m_world.CreateEntity(kGround);
    if (!m_world.IsAlive(groundId)) {
        fail("CreateEntity ground");
        return;
    }
    const std::string groundT = JsonTransform(nlohmann::json::array({ 0.f, 0.f, 0.f }), nlohmann::json::array({ 1.f, 0.f, 0.f, 0.f }),
        nlohmann::json::array({ 12.f, 1.f, 12.f }));
    if (!m_world.SetComponentJson(groundId, "Transform", groundT)) {
        fail("Set Transform ground");
        return;
    }
    const char* groundR = "{\"meshAssetId\":\"builtin://Plane\",\"materialAssetId\":\"\",\"visible\":true,\"submeshIndex\":-1}";
    if (!m_world.SetComponentJson(groundId, "Renderable", groundR)) {
        fail("Set Renderable ground");
        return;
    }

    const auto crateId = m_world.CreateEntity(kCrate);
    if (!m_world.IsAlive(crateId)) {
        fail("CreateEntity crate");
        return;
    }
    const std::string crateT = JsonTransform(nlohmann::json::array({ 0.f, 0.55f, 0.f }), nlohmann::json::array({ 1.f, 0.f, 0.f, 0.f }),
        nlohmann::json::array({ 0.6f, 0.6f, 0.6f }));
    if (!m_world.SetComponentJson(crateId, "Transform", crateT)) {
        fail("Set Transform crate");
        return;
    }
    const char* crateR = "{\"meshAssetId\":\"builtin://Cube\",\"materialAssetId\":\"\",\"visible\":true,\"submeshIndex\":-1}";
    if (!m_world.SetComponentJson(crateId, "Renderable", crateR)) {
        fail("Set Renderable crate");
        return;
    }

    const auto sunId = m_world.CreateEntity(kSun);
    if (!m_world.IsAlive(sunId)) {
        fail("CreateEntity sun");
        return;
    }
    const std::string sunT = JsonTransform(nlohmann::json::array({ 4.f, 10.f, 2.f }), nlohmann::json::array({ 1.f, 0.f, 0.f, 0.f }),
        nlohmann::json::array({ 1.f, 1.f, 1.f }));
    if (!m_world.SetComponentJson(sunId, "Transform", sunT)) {
        fail("Set Transform sun");
        return;
    }
    const char* sunL =
        "{\"type\":0,\"color\":[1.0,0.95,0.85],\"intensity\":1.25,\"range\":100.0,\"innerCone\":15.0,\"outerCone\":45.0,"
        "\"castShadows\":true}";
    if (!m_world.SetComponentJson(sunId, "Light", sunL)) {
        fail("Set Light sun");
        return;
    }

    m_world.FlushTransforms();
    outMessages.push_back("assistant: /ai demo — created \"" + std::string(kGround) + "\", \"" + std::string(kCrate) + "\", \""
        + std::string(kSun) + "\" (plane + cube + directional light). Run /observe or /query to inspect.");
}

void WorldToolBridge::HandleUserLine(const std::string& line, std::vector<std::string>& outMessages) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
        return;
    }

    if (trimmed[0] != '/') {
        outMessages.push_back(
            "assistant: Use **slash commands** (see `/help`). With **OPENAI_API_KEY** set, use `/ask <goal>` for a "
            "natural-language build. Otherwise use `/observe`, `/plan`, or `/ai demo`.");
        return;
    }

    if (trimmed == "/help" || trimmed == "/?") {
        AppendHelp(outMessages);
        return;
    }

    if (trimmed == "/observe") {
        const std::string payload = BuildWorldObservationJson(m_world);
        outMessages.push_back("assistant: /observe (JSON)\n```json\n" + payload + "\n```");
        return;
    }

    const std::string planP = "/plan ";
    if (trimmed.size() >= planP.size() && trimmed.compare(0, planP.size(), planP) == 0) {
        const std::string jsonPart = Trim(trimmed.substr(planP.size()));
        if (jsonPart.empty()) {
            outMessages.push_back(
                "assistant: Usage: /plan [{\"op\":\"create\",\"name\":\"MyEnt\"}, "
                "{\"op\":\"set\",\"name\":\"MyEnt\",\"component\":\"Transform\",\"json\":{...}}, ...]");
            return;
        }
        ExecutePlanFromJsonString(jsonPart, outMessages);
        return;
    }

    if (trimmed == "/ai demo") {
        RunAIDemo(outMessages);
        return;
    }

    if (trimmed == "/query") {
        outMessages.push_back("assistant: /query\n```\n" + QueryWorldSummaryText() + "```");
        return;
    }

    const std::string createP = "/create ";
    if (trimmed.compare(0, createP.size(), createP) == 0) {
        const std::string name = Trim(trimmed.substr(createP.size()));
        if (name.empty()) {
            outMessages.push_back("assistant: Usage: /create <name>");
            return;
        }
        const auto id = m_world.CreateEntity(name);
        if (!m_world.IsAlive(id)) {
            outMessages.push_back("assistant: CreateEntity failed for \"" + name + "\".");
            return;
        }
        m_world.FlushTransforms();
        outMessages.push_back("assistant: Created entity \"" + name + "\" (index=" + std::to_string(id.index) + ").");
        return;
    }

    const std::string destroyP = "/destroy ";
    if (trimmed.compare(0, destroyP.size(), destroyP) == 0) {
        const std::string name = Trim(trimmed.substr(destroyP.size()));
        if (name.empty()) {
            outMessages.push_back("assistant: Usage: /destroy <name>");
            return;
        }
        const auto id = m_world.FindEntityByName(name);
        if (!m_world.IsAlive(id)) {
            outMessages.push_back("assistant: No entity named \"" + name + "\".");
            return;
        }
        if (!m_world.DestroyEntity(id)) {
            outMessages.push_back("assistant: DestroyEntity failed for \"" + name + "\".");
            return;
        }
        m_world.FlushTransforms();
        outMessages.push_back("assistant: Destroyed \"" + name + "\".");
        return;
    }

    std::string ent;
    std::string comp;
    std::string jsonBlob;
    if (ParseGetCommand(trimmed, ent, comp)) {
        const auto id = m_world.FindEntityByName(ent);
        if (!m_world.IsAlive(id)) {
            outMessages.push_back("assistant: No entity \"" + ent + "\".");
            return;
        }
        std::string j;
        if (!m_world.GetComponentJson(id, comp, j)) {
            outMessages.push_back("assistant: GetComponentJson failed (missing component or entity).");
            return;
        }
        outMessages.push_back("assistant: " + comp + " on \"" + ent + "\":\n```json\n" + j + "\n```");
        return;
    }

    if (ParseSetCommand(trimmed, ent, comp, jsonBlob)) {
        const auto id = m_world.FindEntityByName(ent);
        if (!m_world.IsAlive(id)) {
            outMessages.push_back("assistant: No entity \"" + ent + "\".");
            return;
        }
        if (!m_world.SetComponentJson(id, comp, jsonBlob)) {
            outMessages.push_back("assistant: SetComponentJson failed (validation or unknown component).");
            return;
        }
        m_world.FlushTransforms();
        outMessages.push_back("assistant: Set " + comp + " on \"" + ent + "\" OK.");
        return;
    }

    outMessages.push_back("assistant: Unknown command. `/help` for list.");
}

} // namespace openeye::ai_playground
