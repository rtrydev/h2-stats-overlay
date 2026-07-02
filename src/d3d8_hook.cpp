#include "d3d8_hook.h"

#include "d3d8_min.h"
#include "log.h"
#include "overlay_renderer.h"

#include <tlhelp32.h>

#include <cstdint>
#include <cstring>

namespace h2stats::D3D8Hook {
namespace {

using Direct3DCreate8Fn = IDirect3D8*(WINAPI*)(UINT);
using CreateDeviceFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3D8*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice8**);
using EndSceneFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*);
using ResetFn = HRESULT(STDMETHODCALLTYPE*)(IDirect3DDevice8*, D3DPRESENT_PARAMETERS*);

Direct3DCreate8Fn g_originalDirect3DCreate8 = nullptr;
CreateDeviceFn g_originalCreateDevice = nullptr;
EndSceneFn g_originalEndScene = nullptr;
ResetFn g_originalReset = nullptr;
IDirect3DDevice8* g_capturedDevice = nullptr;
bool g_importHookLogged = false;
bool g_createDeviceHookLogged = false;
bool g_endSceneHookLogged = false;

template <typename T>
T VTable(void* object, size_t index) {
    void** table = *reinterpret_cast<void***>(object);
    return reinterpret_cast<T>(table[index]);
}

bool PatchPointer(void** slot, void* replacement, void** original) {
    if (slot == nullptr || replacement == nullptr) {
        return false;
    }

    if (*slot == replacement) {
        return true;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    if (original != nullptr && *slot != replacement) {
        *original = *slot;
    }
    *slot = replacement;
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void*));

    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(void*), oldProtect, &ignored);
    return true;
}

bool PatchVTableSlot(void* object, size_t index, void* replacement, void** original) {
    if (object == nullptr) {
        return false;
    }

    void** table = *reinterpret_cast<void***>(object);
    return PatchPointer(&table[index], replacement, original);
}

void HookDevice(IDirect3DDevice8* device);

HRESULT STDMETHODCALLTYPE HookedReset(IDirect3DDevice8* device, D3DPRESENT_PARAMETERS* presentationParameters) {
    Log::Write("Device reset");
    return g_originalReset(device, presentationParameters);
}

HRESULT STDMETHODCALLTYPE HookedEndScene(IDirect3DDevice8* device) {
    OverlayRenderer::Render(device);
    return g_originalEndScene(device);
}

HRESULT STDMETHODCALLTYPE HookedCreateDevice(IDirect3D8* self,
                                             UINT adapter,
                                             D3DDEVTYPE deviceType,
                                             HWND focusWindow,
                                             DWORD behaviorFlags,
                                             D3DPRESENT_PARAMETERS* presentationParameters,
                                             IDirect3DDevice8** returnedDevice) {
    const HRESULT result = g_originalCreateDevice(self,
                                                  adapter,
                                                  deviceType,
                                                  focusWindow,
                                                  behaviorFlags,
                                                  presentationParameters,
                                                  returnedDevice);
    if (SUCCEEDED(result) && returnedDevice != nullptr && *returnedDevice != nullptr) {
        g_capturedDevice = *returnedDevice;
        Log::Write("Device captured");
        HookDevice(*returnedDevice);
    }
    return result;
}

IDirect3D8* WINAPI HookedDirect3DCreate8(UINT sdkVersion) {
    if (g_originalDirect3DCreate8 == nullptr) {
        return nullptr;
    }

    IDirect3D8* direct3D = g_originalDirect3DCreate8(sdkVersion);
    if (direct3D != nullptr) {
        if (PatchVTableSlot(direct3D, 15, reinterpret_cast<void*>(&HookedCreateDevice), reinterpret_cast<void**>(&g_originalCreateDevice))) {
            if (!g_createDeviceHookLogged) {
                g_createDeviceHookLogged = true;
                Log::Write("CreateDevice hook installed");
            }
        } else {
            Log::Write("Failed to install CreateDevice hook");
        }
    }
    return direct3D;
}

bool IsTargetImport(const char* moduleName) {
    if (moduleName == nullptr) {
        return false;
    }
    return _stricmp(moduleName, "d3d8.dll") == 0;
}

bool PatchModuleImports(HMODULE module) {
    bool patched = false;

    __try {
        auto* base = reinterpret_cast<uint8_t*>(module);
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }

        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) {
            return false;
        }

        const IMAGE_DATA_DIRECTORY& importDirectory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDirectory.VirtualAddress == 0) {
            return false;
        }

        auto* importDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDirectory.VirtualAddress);
        for (; importDescriptor->Name != 0; ++importDescriptor) {
            const char* importedModuleName = reinterpret_cast<const char*>(base + importDescriptor->Name);
            if (!IsTargetImport(importedModuleName)) {
                continue;
            }

            if (importDescriptor->OriginalFirstThunk == 0 || importDescriptor->FirstThunk == 0) {
                continue;
            }

            auto* originalThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + importDescriptor->OriginalFirstThunk);
            auto* firstThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(base + importDescriptor->FirstThunk);
            for (; originalThunk->u1.AddressOfData != 0; ++originalThunk, ++firstThunk) {
                if (IMAGE_SNAP_BY_ORDINAL(originalThunk->u1.Ordinal)) {
                    continue;
                }

                auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + originalThunk->u1.AddressOfData);
                if (strcmp(reinterpret_cast<const char*>(importByName->Name), "Direct3DCreate8") != 0) {
                    continue;
                }

                auto** functionSlot = reinterpret_cast<void**>(&firstThunk->u1.Function);
                void* original = reinterpret_cast<void*>(firstThunk->u1.Function);
                if (original != reinterpret_cast<void*>(&HookedDirect3DCreate8)) {
                    if (g_originalDirect3DCreate8 == nullptr) {
                        g_originalDirect3DCreate8 = reinterpret_cast<Direct3DCreate8Fn>(original);
                    }
                    if (PatchPointer(functionSlot, reinterpret_cast<void*>(&HookedDirect3DCreate8), nullptr)) {
                        patched = true;
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return patched;
}

void HookDevice(IDirect3DDevice8* device) {
    if (device == nullptr) {
        return;
    }

    if (PatchVTableSlot(device, 14, reinterpret_cast<void*>(&HookedReset), reinterpret_cast<void**>(&g_originalReset)) &&
        PatchVTableSlot(device, 35, reinterpret_cast<void*>(&HookedEndScene), reinterpret_cast<void**>(&g_originalEndScene))) {
        if (!g_endSceneHookLogged) {
            g_endSceneHookLogged = true;
            Log::Write("EndScene hook installed");
        }
    } else {
        Log::Write("Failed to install device hooks");
    }
}

} // namespace

void Initialize() {
    Log::Write("D3D8 hook initialization started");
    InstallImportHook();
}

void InstallImportHook() {
    const DWORD processId = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        Log::Write("CreateToolhelp32Snapshot failed: %lu", GetLastError());
        return;
    }

    MODULEENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    bool patchedAny = false;
    if (Module32First(snapshot, &entry)) {
        do {
            if (PatchModuleImports(entry.hModule)) {
                patchedAny = true;
            }
        } while (Module32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);

    if (patchedAny && !g_importHookLogged) {
        g_importHookLogged = true;
        Log::Write("Direct3DCreate8 import hook installed");
    }
}

bool HasCapturedDevice() {
    return g_capturedDevice != nullptr;
}

} // namespace h2stats::D3D8Hook
