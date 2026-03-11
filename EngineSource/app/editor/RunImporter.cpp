#include "RunImporter.h"
#include "api/platform/All.h"

#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace openeye::editor {

namespace fs = eyelib::api::platform::filesystem;

namespace {

std::string GetImporterExePath() {
    std::string exePath = fs::GetExecutablePath();
    if (exePath.empty()) return {};
    std::string dir = fs::GetDirectory(exePath);
    if (dir.empty()) return {};
#ifdef _WIN32
    return fs::NormalizePath(dir + "/AssetImporter.exe");
#else
    return fs::NormalizePath(dir + "/AssetImporter");
#endif
}

} // namespace

bool RunImporter(const std::string& sourcePath, const std::string& outputDir, bool separateMaterials) {
    std::string importerPath = GetImporterExePath();
    if (importerPath.empty() || !fs::FileExists(importerPath))
        return false;

#ifdef _WIN32
    // Build command line: "importerPath" "sourcePath" "outputDir" [--mat]
    std::string cmd;
    auto quote = [](const std::string& s) {
        std::string r = "\"";
        for (char c : s) {
            if (c == '"') r += "\\\"";
            else r += c;
        }
        r += "\"";
        return r;
    };
    cmd = quote(importerPath) + " " + quote(sourcePath) + " " + quote(outputDir);
    if (separateMaterials)
        cmd += " --mat";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(
            nullptr,
            const_cast<char*>(cmd.c_str()),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        return false;
    }
    CloseHandle(pi.hThread);
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 120000); // 2 min
    DWORD exitCode = 1;
    if (waitResult == WAIT_OBJECT_0)
        GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    return (waitResult == WAIT_OBJECT_0 && exitCode == 0);
#else
    (void)sourcePath;
    (void)outputDir;
    (void)separateMaterials;
    // TODO: posix_spawn or fork+exec
    return false;
#endif
}

} // namespace openeye::editor
