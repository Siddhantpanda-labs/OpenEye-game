#pragma once

#include <string>

namespace openeye::ai_playground {

/**
 * HTTPS POST with UTF-8 JSON body (OpenAI-compatible). Windows: WinHTTP.
 * Returns true when the HTTP layer succeeded; \a outHttpStatus is the status code (e.g. 200).
 */
bool HttpPostJsonOpenAI(const std::string& hostUtf8,
                        uint16_t port,
                        const std::string& pathAndQueryUtf8,
                        const std::string& authorizationBearerValue,
                        const std::string& requestBodyUtf8,
                        unsigned timeoutMs,
                        long& outHttpStatus,
                        std::string& responseBodyUtf8,
                        std::string& errorMessage);

} // namespace openeye::ai_playground
