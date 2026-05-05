#pragma once

#include <json.hpp>

#include <functional>
#include <future>
#include <string>
#include <vector>

namespace openeye::ai_playground {

class WorldToolBridge;

/** Outcome of one async HTTPS POST to Gemini generateContent (worker thread). */
struct GeminiHttpRoundOutcome {
    bool transportOk = false;
    long httpStatus = 0;
    std::string responseBody;
    std::string transportError;
};

/**
 * Async Gemini `generateContent` + function calling; tools execute on the main thread in Poll().
 */
class GeminiAskAsyncCoordinator {
public:
    GeminiAskAsyncCoordinator() = default;
    GeminiAskAsyncCoordinator(const GeminiAskAsyncCoordinator&) = delete;
    GeminiAskAsyncCoordinator& operator=(const GeminiAskAsyncCoordinator&) = delete;

    [[nodiscard]] bool IsBusy() const { return m_phase != Phase::Idle; }

    bool TryStart(const std::string& userGoal, const std::string& workspaceRoot, std::vector<std::string>& chatMessages);

    void Poll(WorldToolBridge& bridge, std::vector<std::string>& chatMessages, const std::function<void()>& onWorldMutated);

    void ShutdownJoin();

private:
    enum class Phase { Idle, HttpInFlight };

    void resetSession();
    void removeStatusLine(std::vector<std::string>& chatMessages) const;
    void fail(const std::string& fatal, std::vector<std::string>& chatMessages);
    void launchHttpRound();

    Phase m_phase = Phase::Idle;
    std::future<GeminiHttpRoundOutcome> m_httpFuture;
    size_t m_statusLineIndex = kNoStatusIndex;
    int m_httpLaunches = 0;

    std::string m_apiKey;
    std::string m_model;
    std::string m_systemInstruction;
    nlohmann::json m_contents = nlohmann::json::array();
    nlohmann::json m_toolDeclarations = nlohmann::json::array();

    static constexpr size_t kNoStatusIndex = static_cast<size_t>(-1);
    static constexpr int kMaxToolRounds = 14;
};

} // namespace openeye::ai_playground
