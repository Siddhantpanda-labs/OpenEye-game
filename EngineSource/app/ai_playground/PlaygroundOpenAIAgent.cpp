#include "PlaygroundOpenAIAgent.h"

#include "PlaygroundOpenAIHttp.h"
#include "WorldToolBridge.h"

#include "api/platform/FileSystem.h"

#include <cstdlib>
#include <json.hpp>

namespace openeye::ai_playground {

namespace {

constexpr unsigned kHttpTimeoutMs = 120000;
constexpr int kMaxToolRounds = 14;
constexpr size_t kCatalogMaxChars = 14000;

std::string LoadCatalogExcerpt(const std::string& workspaceRoot) {
    if (workspaceRoot.empty()) {
        return {};
    }
    const std::string path = api::platform::filesystem::NormalizePath(workspaceRoot + "/documentation/Ai_plan/tools_catalog_v1.md");
    if (!api::platform::filesystem::FileExists(path)) {
        return {};
    }
    const std::vector<uint8_t> bytes = api::platform::filesystem::ReadFile(path);
    if (bytes.empty()) {
        return {};
    }
    std::string text(bytes.begin(), bytes.end());
    if (text.size() > kCatalogMaxChars) {
        text.resize(kCatalogMaxChars);
        text += "\n\n[… catalog truncated for token budget …]\n";
    }
    return text;
}

std::string DispatchTool(const std::string& name, const std::string& argumentsJson, WorldToolBridge& bridge) {
    if (name == "observe_world") {
        return bridge.GetObservationJson();
    }
    if (name == "query_world_text") {
        return bridge.QueryWorldSummaryText();
    }
    if (name == "run_world_plan") {
        const nlohmann::json args = nlohmann::json::parse(argumentsJson, nullptr, false);
        if (args.is_discarded()) {
            return R"({"ok":false,"error":"invalid_arguments_json"})";
        }
        if (!args.contains("steps")) {
            return R"({"ok":false,"error":"missing_steps"})";
        }
        const auto& st = args["steps"];
        if (!st.is_array()) {
            return R"({"ok":false,"error":"steps_must_be_array"})";
        }
        return bridge.ApplyPlanToolFromStepsJson(st.dump());
    }
    return nlohmann::json{ { "ok", false }, { "error", "unknown_tool" }, { "name", name } }.dump();
}

nlohmann::json BuildToolsSpec() {
    nlohmann::json tools = nlohmann::json::array();

    nlohmann::json observe;
    observe["type"] = "function";
    observe["function"]["name"] = "observe_world";
    observe["function"]["description"] =
        "Returns a JSON snapshot of the scene: entities, names, and component data. Use when you need full state.";
    observe["function"]["parameters"] = nlohmann::json::object();
    observe["function"]["parameters"]["type"] = "object";
    observe["function"]["parameters"]["properties"] = nlohmann::json::object();
    tools.push_back(std::move(observe));

    nlohmann::json query;
    query["type"] = "function";
    query["function"]["name"] = "query_world_text";
    query["function"]["description"] =
        "Returns a compact text list of entities (id, name, component names only). Prefer this first on large scenes.";
    query["function"]["parameters"] = nlohmann::json::object();
    query["function"]["parameters"]["type"] = "object";
    query["function"]["parameters"]["properties"] = nlohmann::json::object();
    tools.push_back(std::move(query));

    nlohmann::json plan;
    plan["type"] = "function";
    plan["function"]["name"] = "run_world_plan";
    plan["function"]["description"] =
        "Apply an ordered list of world mutations. Each step has op: create | destroy | set. "
        "For set: name, component (e.g. Transform, Renderable, Light), json (object). "
        "Create entities before set. Use builtin://Plane, builtin://Cube for meshAssetId when appropriate.";
    plan["function"]["parameters"] = nlohmann::json::object();
    plan["function"]["parameters"]["type"] = "object";
    plan["function"]["parameters"]["properties"]["steps"]["type"] = "array";
    plan["function"]["parameters"]["properties"]["steps"]["items"]["type"] = "object";
    plan["function"]["parameters"]["required"] = nlohmann::json::array({ "steps" });
    tools.push_back(std::move(plan));

    return tools;
}

std::string SummarizeToolLogLine(const std::string& name, const std::string& result) {
    constexpr size_t kMax = 900;
    if (result.size() <= kMax) {
        return "assistant: [tool] " + name + " → " + result;
    }
    return "assistant: [tool] " + name + " → " + result.substr(0, kMax) + "… (" + std::to_string(result.size()) + " bytes total)";
}

} // namespace

bool RunOpenAIAsk(const std::string& userMessage,
                  const std::string& workspaceRootForCatalog,
                  WorldToolBridge& bridge,
                  std::vector<std::string>& outChatLines,
                  std::string& fatalError) {
    fatalError.clear();
    const char* keyEnv = std::getenv("OPENAI_API_KEY");
    if (keyEnv == nullptr || keyEnv[0] == '\0') {
        fatalError = "OPENAI_API_KEY is not set. Set it in the environment, then use `/ask …` again.";
        return false;
    }
    const std::string apiKey(keyEnv);

    const char* modelEnv = std::getenv("OPENAI_MODEL");
    const std::string model = (modelEnv && modelEnv[0] != '\0') ? std::string(modelEnv) : "gpt-4o-mini";

    std::string system = R"(You control a 3D world through tools only. Rules:
- Prefer query_world_text first if the scene might be large; use observe_world when you need full component JSON.
- Use run_world_plan with an array of steps. Valid ops: create (name), destroy (name), set (name, component, json).
- Transform JSON uses position [x,y,z], rotation quaternion [w,x,y,z], scale [x,y,z].
- Builtin meshes: builtin://Plane, builtin://Cube, builtin://Sphere, etc.
- After tools succeed, briefly summarize what you did for the user.)";

    const std::string catalog = LoadCatalogExcerpt(workspaceRootForCatalog);
    if (!catalog.empty()) {
        system += "\n\n--- Tool catalog excerpt (authoritative) ---\n";
        system += catalog;
    }

    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({ { "role", "system" }, { "content", system } });
    messages.push_back({ { "role", "user" }, { "content", userMessage } });

    const nlohmann::json tools = BuildToolsSpec();

    for (int round = 0; round < kMaxToolRounds; ++round) {
        nlohmann::json body;
        body["model"] = model;
        body["temperature"] = 0.2;
        body["max_tokens"] = 4096;
        body["messages"] = messages;
        body["tools"] = tools;
        body["tool_choice"] = "auto";

        const std::string reqUtf8 = body.dump();
        long httpStatus = 0;
        std::string respUtf8;
        std::string httpErr;
        if (!HttpPostJsonOpenAI("api.openai.com", 443, "/v1/chat/completions", apiKey, reqUtf8, kHttpTimeoutMs, httpStatus,
                respUtf8, httpErr)) {
            fatalError = "HTTP error: " + httpErr;
            return false;
        }

        const nlohmann::json root = nlohmann::json::parse(respUtf8, nullptr, false);
        if (root.is_discarded()) {
            fatalError = "OpenAI response was not valid JSON (HTTP " + std::to_string(httpStatus) + ").";
            return false;
        }

        if (root.contains("error")) {
            fatalError = "OpenAI API error: " + root["error"].dump();
            return false;
        }

        if (httpStatus < 200 || httpStatus >= 300) {
            fatalError = "OpenAI HTTP status " + std::to_string(httpStatus) + ": " + respUtf8.substr(0, 500);
            return false;
        }

        if (!root.contains("choices") || !root["choices"].is_array() || root["choices"].empty()) {
            fatalError = "OpenAI response missing choices.";
            return false;
        }

        const nlohmann::json& choice = root["choices"][0];
        const nlohmann::json msg = choice.value("message", nlohmann::json::object());

        if (msg.contains("tool_calls") && msg["tool_calls"].is_array() && !msg["tool_calls"].empty()) {
            messages.push_back(msg);
            for (const auto& tc : msg["tool_calls"]) {
                if (!tc.contains("id") || !tc.contains("function")) {
                    continue;
                }
                const std::string id = tc["id"].get<std::string>();
                const nlohmann::json& fn = tc["function"];
                const std::string name = fn.value("name", std::string{});
                const std::string args = fn.value("arguments", std::string("{}"));
                const std::string toolResult = DispatchTool(name, args, bridge);
                nlohmann::json tr;
                tr["role"] = "tool";
                tr["tool_call_id"] = id;
                tr["content"] = toolResult;
                messages.push_back(std::move(tr));
                outChatLines.push_back(SummarizeToolLogLine(name, toolResult));
            }
            continue;
        }

        std::string text;
        if (msg.contains("content") && msg["content"].is_string()) {
            text = msg["content"].get<std::string>();
        }
        if (!text.empty()) {
            outChatLines.push_back("assistant: " + text);
        } else {
            outChatLines.push_back("assistant: (model returned no text; finish_reason="
                + choice.value("finish_reason", std::string("?")) + ")");
        }
        return true;
    }

    fatalError = "OpenAI tool loop exceeded max rounds.";
    return false;
}

} // namespace openeye::ai_playground
