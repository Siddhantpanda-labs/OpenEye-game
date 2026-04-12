#include "PlaygroundGeminiCoordinator.h"

#include "PlaygroundOpenAIHttp.h"
#include "WorldToolBridge.h"

#include "api/platform/FileSystem.h"

#include <cctype>
#include <cstdlib>
#include <chrono>
#include <sstream>

namespace openeye::ai_playground {

namespace {

constexpr unsigned kHttpTimeoutMs = 120000;
constexpr const char* kStatusLine = "system: LLM: request in progress...";

std::string TrimAscii(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

std::string LoadCatalogExcerpt(const std::string& workspaceRoot) {
    if (workspaceRoot.empty()) {
        return {};
    }
    constexpr size_t kCatalogMaxChars = 14000;
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

std::string LoadGeminiApiKey(const std::string& workspaceRoot) {
    if (const char* e = std::getenv("GEMINI_API_KEY")) {
        std::string s = TrimAscii(std::string(e));
        if (!s.empty()) {
            return s;
        }
    }
    if (!workspaceRoot.empty()) {
        const std::string path = api::platform::filesystem::NormalizePath(workspaceRoot + "/gemini.key.txt");
        if (api::platform::filesystem::FileExists(path)) {
            const std::vector<uint8_t> bytes = api::platform::filesystem::ReadFile(path);
            if (!bytes.empty()) {
                return TrimAscii(std::string(bytes.begin(), bytes.end()));
            }
        }
    }
    return {};
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

std::string SummarizeToolLogLine(const std::string& name, const std::string& result) {
    constexpr size_t kMax = 900;
    if (result.size() <= kMax) {
        return "assistant: [tool] " + name + " → " + result;
    }
    return "assistant: [tool] " + name + " → " + result.substr(0, kMax) + "… (" + std::to_string(result.size()) + " bytes total)";
}

nlohmann::json BuildGeminiFunctionDeclarations() {
    nlohmann::json decls = nlohmann::json::array();

    nlohmann::json observe;
    observe["name"] = "observe_world";
    observe["description"] =
        "Returns a JSON snapshot of the scene: entities, names, and component data. Use when you need full state.";
    observe["parameters"] = nlohmann::json{ { "type", "object" }, { "properties", nlohmann::json::object() } };
    decls.push_back(std::move(observe));

    nlohmann::json query;
    query["name"] = "query_world_text";
    query["description"] =
        "Returns a compact text list of entities (id, name, component names only). Prefer this first on large scenes.";
    query["parameters"] = nlohmann::json{ { "type", "object" }, { "properties", nlohmann::json::object() } };
    decls.push_back(std::move(query));

    nlohmann::json plan;
    plan["name"] = "run_world_plan";
    plan["description"] =
        "Apply an ordered list of world mutations. Each step has op: create | destroy | set. "
        "For set: name, component (e.g. Transform, Renderable, Light), json (object). "
        "Create entities before set. Use builtin://Plane, builtin://Cube for meshAssetId when appropriate.";
    plan["parameters"] = nlohmann::json::object();
    plan["parameters"]["type"] = "object";
    plan["parameters"]["properties"]["steps"] = nlohmann::json{ { "type", "array" }, { "items", { { "type", "object" } } } };
    plan["parameters"]["required"] = nlohmann::json::array({ "steps" });
    decls.push_back(std::move(plan));

    return decls;
}

std::string ToolArgumentsJsonFromGeminiArgs(const nlohmann::json& args) {
    if (args.is_string()) {
        return args.get<std::string>();
    }
    if (args.is_object() || args.is_array()) {
        return args.dump();
    }
    return std::string("{}");
}

GeminiHttpRoundOutcome PostGeminiGenerateContentBlocking(const std::string& apiKey, const std::string& modelId,
    const std::string& systemInstruction, const nlohmann::json& contents, const nlohmann::json& functionDeclarations) {
    nlohmann::json body;
    body["systemInstruction"]["parts"] = nlohmann::json::array({ nlohmann::json{ { "text", systemInstruction } } });
    body["contents"] = contents;
    body["tools"] = nlohmann::json::array({ nlohmann::json{ { "functionDeclarations", functionDeclarations } } });
    body["toolConfig"]["functionCallingConfig"]["mode"] = "AUTO";
    body["generationConfig"]["temperature"] = 0.2;

    const std::string path = std::string("/v1beta/models/") + modelId + ":generateContent";
    std::ostringstream hdr;
    hdr << "Content-Type: application/json\r\n";
    hdr << "x-goog-api-key: " << apiKey << "\r\n";

    GeminiHttpRoundOutcome o;
    o.transportOk = HttpPostJsonWithHeaders("generativelanguage.googleapis.com", 443, path, hdr.str(), body.dump(), kHttpTimeoutMs,
        o.httpStatus, o.responseBody, o.transportError);
    return o;
}

} // namespace

void GeminiAskAsyncCoordinator::resetSession() {
    m_phase = Phase::Idle;
    m_statusLineIndex = kNoStatusIndex;
    m_httpLaunches = 0;
    m_apiKey.clear();
    m_model.clear();
    m_systemInstruction.clear();
    m_contents = nlohmann::json::array();
    m_toolDeclarations = nlohmann::json::array();
}

void GeminiAskAsyncCoordinator::removeStatusLine(std::vector<std::string>& chatMessages) const {
    if (m_statusLineIndex == kNoStatusIndex || m_statusLineIndex >= chatMessages.size()) {
        return;
    }
    const std::string& line = chatMessages[m_statusLineIndex];
    static const char kPrefix[] = "system: LLM: request in progress";
    const size_t prefixLen = sizeof(kPrefix) - 1u;
    if (line.size() >= prefixLen && line.compare(0, prefixLen, kPrefix) == 0) {
        chatMessages.erase(chatMessages.begin() + static_cast<std::ptrdiff_t>(m_statusLineIndex));
    }
}

void GeminiAskAsyncCoordinator::fail(const std::string& fatal, std::vector<std::string>& chatMessages) {
    removeStatusLine(chatMessages);
    chatMessages.push_back("assistant: " + fatal);
    resetSession();
}

void GeminiAskAsyncCoordinator::launchHttpRound() {
    const std::string apiKeyCopy = m_apiKey;
    const std::string modelCopy = m_model;
    const std::string systemCopy = m_systemInstruction;
    const nlohmann::json contentsCopy = m_contents;
    const nlohmann::json toolsCopy = m_toolDeclarations;

    m_httpFuture = std::async(std::launch::async,
        [apiKeyCopy, modelCopy, systemCopy, contentsCopy, toolsCopy]() -> GeminiHttpRoundOutcome {
            return PostGeminiGenerateContentBlocking(apiKeyCopy, modelCopy, systemCopy, contentsCopy, toolsCopy);
        });
    m_phase = Phase::HttpInFlight;
    ++m_httpLaunches;
}

bool GeminiAskAsyncCoordinator::TryStart(const std::string& userGoal, const std::string& workspaceRoot,
    std::vector<std::string>& chatMessages) {
    if (m_phase != Phase::Idle) {
        chatMessages.push_back("assistant: Wait for the current LLM request to finish.");
        return false;
    }
    if (userGoal.empty()) {
        chatMessages.push_back("assistant: Usage: /ask <natural language goal>");
        return false;
    }

    m_apiKey = LoadGeminiApiKey(workspaceRoot);
    if (m_apiKey.empty()) {
        chatMessages.push_back(
            "assistant: Gemini API key not found. Set **GEMINI_API_KEY** or create **gemini.key.txt** (single line) at the "
            "workspace root next to `EngineSource` / `Projects`, then use `/ask` again.");
        return false;
    }

    const char* modelEnv = std::getenv("GEMINI_MODEL");
    m_model = (modelEnv && modelEnv[0] != '\0') ? TrimAscii(std::string(modelEnv)) : "gemini-2.5-flash";

    std::string system = R"(You control a 3D world through tools only. Rules:
- Prefer query_world_text first if the scene might be large; use observe_world when you need full component JSON.
- Use run_world_plan with an array of steps. Valid ops: create (name), destroy (name), set (name, component, json).
- Transform JSON uses position [x,y,z], rotation quaternion [w,x,y,z], scale [x,y,z].
- Builtin meshes: builtin://Plane, builtin://Cube, builtin://Sphere, etc.
- After tools succeed, briefly summarize what you did for the user.)";

    const std::string catalog = LoadCatalogExcerpt(workspaceRoot);
    if (!catalog.empty()) {
        system += "\n\n--- Tool catalog excerpt (authoritative) ---\n";
        system += catalog;
    }
    m_systemInstruction = std::move(system);
    m_toolDeclarations = BuildGeminiFunctionDeclarations();

    m_contents = nlohmann::json::array();
    m_contents.push_back(nlohmann::json{ { "role", "user" }, { "parts", nlohmann::json::array({ nlohmann::json{ { "text", userGoal } } }) } });

    m_httpLaunches = 0;
    m_statusLineIndex = chatMessages.size();
    chatMessages.push_back(kStatusLine);
    launchHttpRound();
    return true;
}

void GeminiAskAsyncCoordinator::Poll(WorldToolBridge& bridge, std::vector<std::string>& chatMessages,
    const std::function<void()>& onWorldMutated) {
    if (m_phase != Phase::HttpInFlight || !m_httpFuture.valid()) {
        return;
    }

    if (m_httpFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }

    const GeminiHttpRoundOutcome o = m_httpFuture.get();

    if (!o.transportOk) {
        fail("HTTP error: " + o.transportError, chatMessages);
        return;
    }

    const nlohmann::json root = nlohmann::json::parse(o.responseBody, nullptr, false);
    if (root.is_discarded()) {
        fail("Gemini response was not valid JSON (HTTP " + std::to_string(o.httpStatus) + ").", chatMessages);
        return;
    }

    if (root.contains("error")) {
        fail("Gemini API error: " + root["error"].dump(), chatMessages);
        return;
    }

    if (o.httpStatus < 200 || o.httpStatus >= 300) {
        fail("Gemini HTTP status " + std::to_string(o.httpStatus) + ": " + o.responseBody.substr(0, 500), chatMessages);
        return;
    }

    if (!root.contains("candidates") || !root["candidates"].is_array() || root["candidates"].empty()) {
        fail("Gemini response missing candidates.", chatMessages);
        return;
    }

    const nlohmann::json& cand = root["candidates"][0];
    if (cand.contains("content") && cand["content"].contains("parts") && cand["content"]["parts"].is_array()) {
        const nlohmann::json& respParts = cand["content"]["parts"];
        bool anyFunction = false;
        for (const auto& p : respParts) {
            if (p.contains("functionCall")) {
                anyFunction = true;
                break;
            }
        }

        if (anyFunction) {
            nlohmann::json modelTurn = nlohmann::json::object();
            modelTurn["role"] = "model";
            modelTurn["parts"] = respParts;
            m_contents.push_back(std::move(modelTurn));

            nlohmann::json userTurn;
            userTurn["role"] = "user";
            nlohmann::json frParts = nlohmann::json::array();
            bool anyTool = false;
            static const nlohmann::json kEmptyArgs = nlohmann::json::object();
            for (const auto& p : respParts) {
                if (!p.contains("functionCall")) {
                    continue;
                }
                const nlohmann::json& fc = p["functionCall"];
                const std::string name = fc.value("name", std::string{});
                const nlohmann::json& args =
                    fc.contains("args") ? fc["args"] : (fc.contains("arguments") ? fc["arguments"] : kEmptyArgs);
                const std::string argsJson = ToolArgumentsJsonFromGeminiArgs(args);
                const std::string toolResult = DispatchTool(name, argsJson, bridge);

                nlohmann::json frPart;
                frPart["functionResponse"]["name"] = name;
                nlohmann::json parsed = nlohmann::json::parse(toolResult, nullptr, false);
                if (!parsed.is_discarded()) {
                    frPart["functionResponse"]["response"] = std::move(parsed);
                } else {
                    frPart["functionResponse"]["response"] = nlohmann::json{ { "text", toolResult } };
                }
                frParts.push_back(std::move(frPart));
                chatMessages.push_back(SummarizeToolLogLine(name, toolResult));
                anyTool = true;
            }

            if (frParts.empty()) {
                fail("Gemini returned functionCall parts that could not be handled.", chatMessages);
                return;
            }

            userTurn["parts"] = std::move(frParts);
            m_contents.push_back(std::move(userTurn));

            if (anyTool) {
                onWorldMutated();
            }
            if (m_httpLaunches >= kMaxToolRounds) {
                fail("Gemini tool loop exceeded max rounds.", chatMessages);
                return;
            }
            launchHttpRound();
            return;
        }

        std::string text;
        for (const auto& p : respParts) {
            if (p.contains("text") && p["text"].is_string()) {
                text += p["text"].get<std::string>();
            }
        }
        removeStatusLine(chatMessages);
        if (!text.empty()) {
            chatMessages.push_back("assistant: " + text);
        } else {
            const std::string fr = cand.value("finishReason", std::string("?"));
            chatMessages.push_back("assistant: (Gemini returned no text; finishReason=" + fr + ")");
        }
        onWorldMutated();
        resetSession();
        return;
    }

    fail("Gemini candidate had no usable content.parts.", chatMessages);
}

void GeminiAskAsyncCoordinator::ShutdownJoin() {
    if (m_httpFuture.valid()) {
        m_httpFuture.wait();
        (void)m_httpFuture.get();
    }
    m_httpFuture = {};
    resetSession();
}

} // namespace openeye::ai_playground
