#include "PlaygroundOpenAIHttp.h"

#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#endif

namespace openeye::ai_playground {

namespace {

#ifdef _WIN32
std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring w(static_cast<size_t>(n), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.c_str(), static_cast<int>(utf8.size()), w.data(), n) <= 0) {
        return {};
    }
    return w;
}
#endif

} // namespace

bool HttpPostJsonWithHeaders(const std::string& hostUtf8,
                             const uint16_t port,
                             const std::string& pathAndQueryUtf8,
                             const std::string& headerBlockUtf8,
                             const std::string& requestBodyUtf8,
                             const unsigned timeoutMs,
                             long& outHttpStatus,
                             std::string& responseBodyUtf8,
                             std::string& errorMessage) {
    outHttpStatus = 0;
    responseBodyUtf8.clear();
    errorMessage.clear();

#ifdef _WIN32
    const std::wstring hostW = Utf8ToWide(hostUtf8);
    const std::wstring pathW = Utf8ToWide(pathAndQueryUtf8);
    if (hostW.empty() || pathW.empty()) {
        errorMessage = "HttpPostJsonWithHeaders: invalid host or path (UTF-8 conversion failed).";
        return false;
    }

    HINTERNET hSession =
        WinHttpOpen(L"OpenEyeAIPlayground/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        errorMessage = "WinHttpOpen failed.";
        return false;
    }

    const DWORD timeout = timeoutMs == 0 ? 120000u : static_cast<DWORD>(timeoutMs);
    WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);

    HINTERNET hConnect = WinHttpConnect(hSession, hostW.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        errorMessage = "WinHttpConnect failed.";
        return false;
    }

    const DWORD flags = WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", pathW.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        errorMessage = "WinHttpOpenRequest failed.";
        return false;
    }

    const std::wstring hdrW = Utf8ToWide(headerBlockUtf8);
    const wchar_t* hdrPtr = hdrW.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdrW.c_str();
    const DWORD hdrLen = hdrW.empty() ? 0u : static_cast<DWORD>(-1);

    const BOOL sent = WinHttpSendRequest(hRequest, hdrPtr, hdrLen, (LPVOID)requestBodyUtf8.data(),
        static_cast<DWORD>(requestBodyUtf8.size()), static_cast<DWORD>(requestBodyUtf8.size()), 0);
    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        errorMessage = "WinHttpSendRequest failed.";
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        errorMessage = "WinHttpReceiveResponse failed.";
        return false;
    }

    DWORD status = 0;
    DWORD sz = sizeof(status);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status,
            &sz, WINHTTP_NO_HEADER_INDEX)) {
        outHttpStatus = static_cast<long>(status);
    }

    std::string response;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &available)) {
            break;
        }
        if (available == 0) {
            break;
        }
        const size_t oldSize = response.size();
        response.resize(oldSize + static_cast<size_t>(available));
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, response.data() + oldSize, available, &read)) {
            response.resize(oldSize);
            break;
        }
        response.resize(oldSize + static_cast<size_t>(read));
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    responseBodyUtf8 = std::move(response);
    return true;
#else
    (void)hostUtf8;
    (void)port;
    (void)pathAndQueryUtf8;
    (void)headerBlockUtf8;
    (void)requestBodyUtf8;
    (void)timeoutMs;
    errorMessage = "HTTPS JSON POST is only implemented on Windows (WinHTTP). Build on Windows or add a portable client.";
    return false;
#endif
}

bool HttpPostJsonOpenAI(const std::string& hostUtf8,
                        const uint16_t port,
                        const std::string& pathAndQueryUtf8,
                        const std::string& authorizationBearerValue,
                        const std::string& requestBodyUtf8,
                        const unsigned timeoutMs,
                        long& outHttpStatus,
                        std::string& responseBodyUtf8,
                        std::string& errorMessage) {
    std::ostringstream hdr;
    hdr << "Content-Type: application/json\r\n";
    hdr << "Authorization: Bearer " << authorizationBearerValue << "\r\n";
    return HttpPostJsonWithHeaders(hostUtf8, port, pathAndQueryUtf8, hdr.str(), requestBodyUtf8, timeoutMs, outHttpStatus,
        responseBodyUtf8, errorMessage);
}

} // namespace openeye::ai_playground
