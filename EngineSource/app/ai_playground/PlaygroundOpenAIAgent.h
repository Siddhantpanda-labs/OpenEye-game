#pragma once

#include <string>
#include <vector>

namespace openeye::ai_playground {

class WorldToolBridge;

/**
 * Runs one OpenAI Chat Completions turn with tools (observe, query text, run_world_plan).
 * Requires `OPENAI_API_KEY` in the environment. Optional: `OPENAI_MODEL` (default `gpt-4o-mini`).
 * Appends assistant/system lines to \a outChatLines (same style as slash commands).
 * On failure sets \a fatalError (single line; no key material).
 */
bool RunOpenAIAsk(const std::string& userMessage,
                  const std::string& workspaceRootForCatalog,
                  WorldToolBridge& bridge,
                  std::vector<std::string>& outChatLines,
                  std::string& fatalError);

} // namespace openeye::ai_playground
