#include "game_reader.h"

#include "log.h"

#include <windows.h>

#include <cstring>

namespace h2stats {
namespace {

DWORD g_lastReadFailureLogTick = 0;
DWORD g_lastDegradedReadLogTick = 0;
char g_lastMissionKey[9] = {};

} // namespace

const MissionInfo* FindMission(const char* key, const MissionInfo* table, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (memcmp(key, table[index].key, 8) == 0) {
            return &table[index];
        }
    }
    return nullptr;
}

bool IsAllZeroKey(const char* key) {
    for (int index = 0; index < 8; ++index) {
        if (key[index] != '\0') {
            return false;
        }
    }
    return true;
}

void LogReadFailure(const char* context) {
    const DWORD now = GetTickCount();
    if (now - g_lastReadFailureLogTick < 5000) {
        return;
    }
    g_lastReadFailureLogTick = now;
    Log::Write("Memory read failed while reading %s", context);
}

void LogDegradedRead(const char* context) {
    const DWORD now = GetTickCount();
    if (now - g_lastDegradedReadLogTick < 5000) {
        return;
    }
    g_lastDegradedReadLogTick = now;
    Log::Write("Continuing with degraded stat read: %s", context);
}

void LogMissionChange(const StatsSnapshot& snapshot) {
    if (memcmp(g_lastMissionKey, snapshot.missionKey, 8) != 0) {
        memcpy(g_lastMissionKey, snapshot.missionKey, 9);
        Log::Write("Mission detected: #%d %s (%s)",
                   snapshot.missionNumber,
                   snapshot.missionName,
                   snapshot.missionKey);
    }
}

} // namespace h2stats
