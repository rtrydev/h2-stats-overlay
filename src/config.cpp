#include "config.h"

#include "log.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace h2stats::Config {
namespace {

CRITICAL_SECTION g_lock;
bool g_lockInitialized = false;
OverlayConfig g_config;
char g_configPath[MAX_PATH] = {};
FILETIME g_lastWriteTime = {};

constexpr const char* kDefaultIni =
    "[Overlay]\r\n"
    "Enabled=1\r\n"
    "OffsetX=34\r\n"
    "OffsetY=150\r\n"
    "Scale=1.0\r\n"
    "LineSpacing=18\r\n"
    "ShowInMenus=0\r\n"
    "ShowTimer=1\r\n"
    "Verbose=1\r\n"
    "VerboseScale=0.75\r\n"
    "VerboseLineSpacing=12\r\n"
    "\r\n"
    "[Debug]\r\n"
    "Log=1\r\n";

void AppendPath(char* target, size_t targetSize, const char* dir, const char* file) {
    strcpy_s(target, targetSize, dir);
    const size_t len = strlen(target);
    if (len > 0 && target[len - 1] != '\\' && target[len - 1] != '/') {
        strcat_s(target, targetSize, "\\");
    }
    strcat_s(target, targetSize, file);
}

void EnsureDefaultConfigFile() {
    if (GetFileAttributesA(g_configPath) != INVALID_FILE_ATTRIBUTES) {
        return;
    }

    HANDLE file = CreateFileA(g_configPath,
                              GENERIC_WRITE,
                              FILE_SHARE_READ,
                              nullptr,
                              CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        Log::Write("Failed to create default config: %lu", GetLastError());
        return;
    }

    DWORD bytesWritten = 0;
    WriteFile(file, kDefaultIni, static_cast<DWORD>(strlen(kDefaultIni)), &bytesWritten, nullptr);
    CloseHandle(file);
}

float ReadFloat(const char* section, const char* key, float fallback) {
    char buffer[64] = {};
    char fallbackText[64] = {};
    snprintf(fallbackText, sizeof(fallbackText), "%.3f", fallback);
    GetPrivateProfileStringA(section, key, fallbackText, buffer, sizeof(buffer), g_configPath);
    char* end = nullptr;
    const float value = static_cast<float>(strtod(buffer, &end));
    return end == buffer ? fallback : value;
}

void CaptureLastWriteTime() {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (GetFileAttributesExA(g_configPath, GetFileExInfoStandard, &data)) {
        g_lastWriteTime = data.ftLastWriteTime;
    }
}

void LoadLocked(bool announce) {
    OverlayConfig loaded;
    loaded.enabled = GetPrivateProfileIntA("Overlay", "Enabled", 1, g_configPath) != 0;
    loaded.offsetX = GetPrivateProfileIntA("Overlay", "OffsetX", 34, g_configPath);
    loaded.offsetY = GetPrivateProfileIntA("Overlay", "OffsetY", 150, g_configPath);
    loaded.scale = ReadFloat("Overlay", "Scale", 1.0f);
    loaded.lineSpacing = GetPrivateProfileIntA("Overlay", "LineSpacing", 18, g_configPath);
    loaded.showInMenus = GetPrivateProfileIntA("Overlay", "ShowInMenus", 0, g_configPath) != 0;
    loaded.showTimer = GetPrivateProfileIntA("Overlay", "ShowTimer", 1, g_configPath) != 0;
    loaded.verbose = GetPrivateProfileIntA("Overlay", "Verbose", 1, g_configPath) != 0;
    loaded.verboseScale = ReadFloat("Overlay", "VerboseScale", 0.75f);
    loaded.verboseLineSpacing = GetPrivateProfileIntA("Overlay", "VerboseLineSpacing", 12, g_configPath);
    loaded.debugLog = GetPrivateProfileIntA("Debug", "Log", 1, g_configPath) != 0;

    if (loaded.scale < 0.5f) {
        loaded.scale = 0.5f;
    }
    if (loaded.scale > 4.0f) {
        loaded.scale = 4.0f;
    }
    if (loaded.lineSpacing < 8) {
        loaded.lineSpacing = 8;
    }
    if (loaded.verboseScale < 0.5f) {
        loaded.verboseScale = 0.5f;
    }
    if (loaded.verboseScale > 2.0f) {
        loaded.verboseScale = 2.0f;
    }
    if (loaded.verboseLineSpacing < 8) {
        loaded.verboseLineSpacing = 8;
    }

    g_config = loaded;
    Log::SetEnabled(loaded.debugLog);
    CaptureLastWriteTime();

    if (announce) {
        Log::Write("Config loaded: enabled=%d offset=(%d,%d) scale=%.2f lineSpacing=%d showInMenus=%d showTimer=%d verbose=%d",
                   loaded.enabled ? 1 : 0,
                   loaded.offsetX,
                   loaded.offsetY,
                   loaded.scale,
                   loaded.lineSpacing,
                   loaded.showInMenus ? 1 : 0,
                   loaded.showTimer ? 1 : 0,
                   loaded.verbose ? 1 : 0);
    }
}

} // namespace

void Initialize(const char* moduleDirectory) {
    if (!g_lockInitialized) {
        InitializeCriticalSection(&g_lock);
        g_lockInitialized = true;
    }

    AppendPath(g_configPath, sizeof(g_configPath), moduleDirectory, "h2_stats_overlay.ini");
    EnsureDefaultConfigFile();

    EnterCriticalSection(&g_lock);
    LoadLocked(true);
    LeaveCriticalSection(&g_lock);
}

void ReloadIfChanged() {
    WIN32_FILE_ATTRIBUTE_DATA data = {};
    if (!GetFileAttributesExA(g_configPath, GetFileExInfoStandard, &data)) {
        return;
    }

    EnterCriticalSection(&g_lock);
    if (CompareFileTime(&data.ftLastWriteTime, &g_lastWriteTime) != 0) {
        LoadLocked(true);
    }
    LeaveCriticalSection(&g_lock);
}

OverlayConfig Get() {
    EnterCriticalSection(&g_lock);
    const OverlayConfig copy = g_config;
    LeaveCriticalSection(&g_lock);
    return copy;
}

const char* Path() {
    return g_configPath;
}

} // namespace h2stats::Config
