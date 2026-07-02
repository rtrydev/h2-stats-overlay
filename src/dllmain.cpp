#include "config.h"
#include "d3d8_hook.h"
#include "log.h"

#include <windows.h>

namespace {

HMODULE g_module = nullptr;

DWORD WINAPI InitThread(LPVOID) {
    h2stats::Log::Initialize(g_module);
    h2stats::Config::Initialize(h2stats::Log::ModuleDirectory());
    h2stats::D3D8Hook::Initialize();

    for (int attempt = 0; attempt < 300 && !h2stats::D3D8Hook::HasCapturedDevice(); ++attempt) {
        h2stats::D3D8Hook::InstallImportHook();
        Sleep(100);
    }

    if (!h2stats::D3D8Hook::HasCapturedDevice()) {
        h2stats::Log::Write("Device was not captured during initialization window");
    }
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}

