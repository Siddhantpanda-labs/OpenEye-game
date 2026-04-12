#pragma once

#include <json.hpp>

#include <functional>
#include <future>
#include <string>
#include <vector>

namespace openeye::ai_playground {

class WorldToolBridge;

/** Result of one async HTTPS POST to chat/completions (worker thread). */
struct OpenAiHttpRoundOutcome {
    bool transportOk = false;
    long httpStatus = 0;
    std::string responseBody;
    std::string transportError;
};

/**
 * Drives OpenAI Chat Completions + tools without blocking the main thread.
 * HTTP runs on a worker (std::async); each Poll() on the game thread consumes results,
 * runs tools against WorldToolBridge, and may start the next HTTP round.
 */
class OpenAiAskAsyncCoordinator {
public:
    OpenAiAskAsyncCoordinator() = default;
    OpenAiAskAsyncCoordinator(const OpenAiAskAsyncCoordinator&) = delete;
    OpenAiAskAsyncCoordinator& operator=(const OpenAiAskAsyncCoordinator&) = delete;

    [[nodiscard]] bool IsBusy() const { return m_phase != Phase::Idle; }

    /**
     * Begin a new /ask turn. Fails if one is already in flight or prerequisites missing.
     * On success appends "system: OpenAI: request in progress…" to \a chatMessages (caller already logged the user line).
     */
    bool TryStart(const std::string& userGoal, const std::string& workspaceRoot, std::vector<std::string>& chatMessages);

    /** Call each frame from the main thread while IsBusy() or until idle. */
    void Poll(WorldToolBridge& bridge, std::vector<std::string>& chatMessages, const std::function<void()>& onWorldMutated);

    /** Wait for any in-flight HTTP to finish without applying results (app shutdown). */
    void ShutdownJoin();

private:
    enum class Phase { Idle, HttpInFlight };

    void resetSession();
    void removeStatusLine(std::vector<std::string>& chatMessages) const;
    void fail(const std::string& fatal, std::vector<std::string>& chatMessages);
    void launchHttpRound();

    Phase m_phase = Phase::Idle;
    std::future<OpenAiHttpRoundOutcome> m_httpFuture;
    size_t m_statusLineIndex = kNoStatusIndex;
    /** Count of HTTP requests started (TryStart + each follow-up after tool_calls); cap matches legacy sync loop. */
    int m_httpLaunches = 0;

    std::string m_model;
    std::string m_apiKey;
    nlohmann::json m_messages = nlohmann::json::array();
    nlohmann::json m_tools = nlohmann::json::array();

    static constexpr size_t kNoStatusIndex = static_cast<size_t>(-1);
};

} // namespace openeye::ai_playground
