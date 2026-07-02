#include "log.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace h2stats::Log {
namespace {

CRITICAL_SECTION g_lock;
bool g_lockInitialized = false;
bool g_initialized = false;
bool g_enabled = true;
char g_moduleDirectory[MAX_PATH] = {};
char g_logPath[MAX_PATH] = {};

void AppendPath(char* target, size_t targetSize, const char* dir, const char* file) {
    strcpy_s(target, targetSize, dir);
    const size_t len = strlen(target);
    if (len > 0 && target[len - 1] != '\\' && target[len - 1] != '/') {
        strcat_s(target, targetSize, "\\");
    }
    strcat_s(target, targetSize, file);
}

} // namespace

void Initialize(HMODULE module) {
    if (!g_lockInitialized) {
        InitializeCriticalSection(&g_lock);
        g_lockInitialized = true;
    }

    char modulePath[MAX_PATH] = {};
    GetModuleFileNameA(module, modulePath, MAX_PATH);
    strcpy_s(g_moduleDirectory, modulePath);

    char* slash = strrchr(g_moduleDirectory, '\\');
    if (slash == nullptr) {
        slash = strrchr(g_moduleDirectory, '/');
    }
    if (slash != nullptr) {
        *slash = '\0';
    }

    AppendPath(g_logPath, sizeof(g_logPath), g_moduleDirectory, "h2_stats_overlay.log");
    g_initialized = true;

    FILE* file = nullptr;
    if (fopen_s(&file, g_logPath, "a") == 0 && file != nullptr) {
        SYSTEMTIME time = {};
        GetLocalTime(&time);
        std::fprintf(file,
                     "\n[%04u-%02u-%02u %02u:%02u:%02u] h2_stats_overlay loaded\n",
                     time.wYear,
                     time.wMonth,
                     time.wDay,
                     time.wHour,
                     time.wMinute,
                     time.wSecond);
        std::fclose(file);
    }
}

void SetEnabled(bool enabled) {
    g_enabled = enabled;
}

void Write(const char* format, ...) {
    if (!g_initialized || !g_enabled) {
        return;
    }

    char message[1024] = {};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    EnterCriticalSection(&g_lock);
    FILE* file = nullptr;
    if (fopen_s(&file, g_logPath, "a") == 0 && file != nullptr) {
        SYSTEMTIME time = {};
        GetLocalTime(&time);
        std::fprintf(file,
                     "[%02u:%02u:%02u] %s\n",
                     time.wHour,
                     time.wMinute,
                     time.wSecond,
                     message);
        std::fclose(file);
    }
    LeaveCriticalSection(&g_lock);
}

const char* ModuleDirectory() {
    return g_moduleDirectory;
}

} // namespace h2stats::Log

