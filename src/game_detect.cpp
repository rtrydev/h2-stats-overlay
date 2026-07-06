#include "game_detect.h"

#include <windows.h>

#include <cctype>
#include <cstring>

namespace h2stats {

Game DetectGame() {
    char path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (length == 0 || length >= sizeof(path)) {
        return Game::Unknown;
    }

    const char* fileName = path;
    for (const char* cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') {
            fileName = cursor + 1;
        }
    }

    char lowered[MAX_PATH] = {};
    size_t index = 0;
    for (; fileName[index] != '\0' && index < sizeof(lowered) - 1; ++index) {
        lowered[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(fileName[index])));
    }
    lowered[index] = '\0';

    if (strstr(lowered, "contracts") != nullptr || strstr(lowered, "hitman3") != nullptr) {
        return Game::Contracts;
    }
    if (strstr(lowered, "hitman2") != nullptr) {
        return Game::Hitman2;
    }
    return Game::Unknown;
}

} // namespace h2stats
