#include "EditorFolderPicker.h"
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl_core.h>
#include <comdef.h>
#endif

namespace openeye::editor {

#ifdef _WIN32
static std::string WideToUtf8(const wchar_t* wstr) {
    if (!wstr || !*wstr) return {};
    const int wideLen = static_cast<int>(wcslen(wstr));
    if (wideLen <= 0) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wstr, wideLen, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, wideLen, &out[0], size, nullptr, nullptr);
    return out;
}
#endif

std::string ShowFolderPickerDialog() {
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninit = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;

    std::string result;
    IFileDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        DWORD opts = 0;
        if (SUCCEEDED(pfd->GetOptions(&opts))) {
            pfd->SetOptions(opts | FOS_PICKFOLDERS);
        }
        if (SUCCEEDED(pfd->Show(nullptr))) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR pathW = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pathW)) && pathW) {
                    result = WideToUtf8(pathW);
                    CoTaskMemFree(pathW);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    if (needUninit)
        CoUninitialize();
    return result;
#else
    (void)0;
    return {};
#endif
}

} // namespace openeye::editor
