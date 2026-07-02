#include "stats_reader.h"

#include "log.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>

namespace h2stats::StatsReader {
namespace {

constexpr uintptr_t kPreferredBaseAddress = 0x00400000;

constexpr int kSecondOffsets[] = {
    0x838, 0xB24, 0x8A0, 0x138, 0xB88,
    0xBB8, 0xB48, 0xCE8, 0x136C, 0xAD0,
    0xF50, 0x8D4, 0x9EC, 0x400, 0x9EC,
    0x644, 0xB08, 0x96C, 0xB00, 0x8
};

struct MissionInfo {
    const char key[9];
    const char* name;
    int number;
};

constexpr MissionInfo kMissions[] = {
    {"C1-1__MA", "Anathema", 1},
    {"C2-1__MA", "St. Petersburg Stakeout", 2},
    {"C2-2__MA", "Kirov Park Meeting", 3},
    {"C2-3__MA", "Tubeway Torpedo", 4},
    {"C2-4__MA", "Invitation to a Party", 5},
    {"C3-1__MA", "Tracking Hayamoto", 6},
    {"\\C3-2a__", "Hidden Valley", 7},
    {"\\C3-2b__", "At the Gates", 8},
    {"C3-3__MA", "Shogun Showdown", 9},
    {"C4-1__MA", "Basement Killing", 10},
    {"C4-2__MA", "The Graveyard Shift", 11},
    {"C4-3__MA", "The Jacuzzi Job", 12},
    {"C5-1__MA", "Murder At The Bazaar", 13},
    {"C5-2__MA", "The Motorcade Interception", 14},
    {"C5-3__MA", "Tunnel Rat", 15},
    {"C6-1__MA", "Temple City Ambush", 16},
    {"C6-2__MA", "The Death of Hannelore", 17},
    {"C6-3__MA", "Terminal Hospitality", 18},
    {"C7-1__MA", "St. Petersburg Revisited", 19},
    {"C8-1__MA", "Redemption at Gontranno", 20},
};

DWORD g_lastReadFailureLogTick = 0;
DWORD g_lastDegradedReadLogTick = 0;
char g_lastMissionKey[9] = {};

struct ShotCandidate {
    uintptr_t address = 0;
    int initialValue = 0;
    int lastValue = 0;
};

std::vector<ShotCandidate> g_shotCandidates;
int g_dynamicShotValue = 0;
int g_lastMissionTime = 0;

uintptr_t GameBase() {
    const HMODULE exe = GetModuleHandleA(nullptr);
    if (exe == nullptr) {
        return kPreferredBaseAddress;
    }
    return reinterpret_cast<uintptr_t>(exe);
}

template <typename T>
bool ReadValue(uintptr_t address, T& value) {
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(GetCurrentProcess(),
                             reinterpret_cast<LPCVOID>(address),
                             &value,
                             sizeof(T),
                             &bytesRead) != FALSE &&
           bytesRead == sizeof(T);
}

bool ResolvePointerAddress(uintptr_t address, const int* offsets, size_t offsetCount, uintptr_t& resolvedAddress) {
    uintptr_t current = address;
    for (size_t index = 0; index < offsetCount; ++index) {
        uint32_t next = 0;
        if (!ReadValue(current, next) || next == 0) {
            return false;
        }
        current = static_cast<uintptr_t>(next) + static_cast<uintptr_t>(offsets[index]);
    }

    resolvedAddress = current;
    return true;
}

template <typename T>
bool ReadPointerValue(uintptr_t address, const int* offsets, size_t offsetCount, T& value) {
    uintptr_t resolvedAddress = 0;
    if (!ResolvePointerAddress(address, offsets, offsetCount, resolvedAddress)) {
        return false;
    }

    return ReadValue(resolvedAddress, value);
}

const MissionInfo* FindMission(const char* key) {
    for (const MissionInfo& mission : kMissions) {
        if (memcmp(key, mission.key, 8) == 0) {
            return &mission;
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

bool ReadCounter(uintptr_t base, int missionNumber, int statOffset, int& value) {
    if (missionNumber < 1 || missionNumber > static_cast<int>(sizeof(kSecondOffsets) / sizeof(kSecondOffsets[0]))) {
        return false;
    }

    const int offsets[] = {0x28, kSecondOffsets[missionNumber - 1], statOffset};
    return ReadPointerValue(base + 0x2A6C50, offsets, sizeof(offsets) / sizeof(offsets[0]), value);
}

bool ReadCounterOrZero(uintptr_t base, int missionNumber, int statOffset, const char* name, int& value) {
    if (ReadCounter(base, missionNumber, statOffset, value)) {
        return true;
    }

    value = 0;
    LogDegradedRead(name);
    return false;
}

bool TryReadShotsFromMissionState(uintptr_t base, int& value) {
    const int timeOffsets[] = {0x118, 0xB38, 0x8, 0x1084, 0x24};
    uintptr_t missionTimeAddress = 0;
    if (!ResolvePointerAddress(base + 0x2A6C58,
                               timeOffsets,
                               sizeof(timeOffsets) / sizeof(timeOffsets[0]),
                               missionTimeAddress)) {
        return false;
    }

    int missionTime = 0;
    if (!ReadValue(missionTimeAddress, missionTime) || missionTime <= 0) {
        return false;
    }

    // The old shots pointer is stale in current installs, but the live mission timer
    // owner points to a companion stats panel structure containing the shot counters.
    if (missionTimeAddress < 0x34) {
        return false;
    }

    uint32_t shotTable = 0;
    if (!ReadValue(missionTimeAddress - 0x30, shotTable) || shotTable == 0) {
        return false;
    }

    uint32_t shotStats = 0;
    if (!ReadValue(static_cast<uintptr_t>(shotTable) + 0x54, shotStats) || shotStats == 0) {
        return false;
    }

    int mirroredMissionTime = 0;
    int shotsA = 0;
    int shotsB = 0;
    if (!ReadValue(static_cast<uintptr_t>(shotStats) + 0x24, mirroredMissionTime) ||
        !ReadValue(static_cast<uintptr_t>(shotStats) + 0x34, shotsA) ||
        !ReadValue(static_cast<uintptr_t>(shotStats) + 0x38, shotsB)) {
        return false;
    }

    const int timeDelta = mirroredMissionTime > missionTime
                              ? mirroredMissionTime - missionTime
                              : missionTime - mirroredMissionTime;
    if (mirroredMissionTime <= 0 || timeDelta > 10) {
        return false;
    }
    if (shotsA < 0 || shotsA > 500 || shotsB < 0 || shotsB > 500) {
        return false;
    }

    value = shotsA > shotsB ? shotsA : shotsB;
    return true;
}

bool IsReadableMemory(const MEMORY_BASIC_INFORMATION& info) {
    if (info.State != MEM_COMMIT) {
        return false;
    }
    if ((info.Protect & PAGE_NOACCESS) != 0 || (info.Protect & PAGE_GUARD) != 0) {
        return false;
    }
    return true;
}

bool LooksLikeShotCounterStruct(const uint8_t* bytes, size_t available) {
    if (available < 0x50) {
        return false;
    }

    const auto readInt = [bytes](size_t offset) {
        int value = 0;
        memcpy(&value, bytes + offset, sizeof(value));
        return value;
    };

    const int shots = readInt(0x00);
    if (shots < 0 || shots > 500) {
        return false;
    }
    if (readInt(0x04) != 4 || readInt(0x08) != 17) {
        return false;
    }
    for (size_t offset = 0x0C; offset <= 0x4C; offset += 4) {
        if (readInt(offset) != 0) {
            return false;
        }
    }
    return true;
}

void ResetDynamicShots() {
    g_shotCandidates.clear();
    g_dynamicShotValue = 0;
    g_lastMissionTime = 0;
}

void ScanShotCandidates() {
    g_shotCandidates.clear();

    uintptr_t address = 0x00400000;
    constexpr uintptr_t kMaxAddress = 0x30000000;

    while (address < kMaxAddress) {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0) {
            address += 0x10000;
            continue;
        }

        const uintptr_t regionBase = reinterpret_cast<uintptr_t>(info.BaseAddress);
        const uintptr_t regionEnd = regionBase + info.RegionSize;
        const uintptr_t nextAddress = regionEnd > address ? regionEnd : address + 0x1000;

        if (IsReadableMemory(info) && info.RegionSize <= 64 * 1024 * 1024) {
            constexpr size_t kChunkSize = 1024 * 1024;
            for (uintptr_t chunk = regionBase; chunk < regionEnd; chunk += kChunkSize) {
                const size_t bytesToRead = static_cast<size_t>((regionEnd - chunk) < kChunkSize ? (regionEnd - chunk) : kChunkSize);
                std::vector<uint8_t> buffer(bytesToRead);
                SIZE_T bytesRead = 0;
                if (!ReadProcessMemory(GetCurrentProcess(),
                                       reinterpret_cast<LPCVOID>(chunk),
                                       buffer.data(),
                                       bytesToRead,
                                       &bytesRead) ||
                    bytesRead < 0x50) {
                    continue;
                }

                const uintptr_t alignedStart = (chunk + 3) & ~static_cast<uintptr_t>(3);
                for (uintptr_t candidate = alignedStart; candidate + 0x50 <= chunk + bytesRead; candidate += 4) {
                    const size_t offset = static_cast<size_t>(candidate - chunk);
                    if (!LooksLikeShotCounterStruct(buffer.data() + offset, bytesRead - offset)) {
                        continue;
                    }

                    int value = 0;
                    memcpy(&value, buffer.data() + offset, sizeof(value));
                    g_shotCandidates.push_back({candidate, value, value});
                }
            }
        }

        address = nextAddress;
    }

    Log::Write("Shot counter candidate scan found %u candidates", static_cast<unsigned>(g_shotCandidates.size()));
}

bool TryReadDynamicShots(int& value) {
    if (g_shotCandidates.empty()) {
        ScanShotCandidates();
    }

    if (g_shotCandidates.empty()) {
        return false;
    }

    int bestDelta = g_dynamicShotValue;
    uintptr_t bestAddress = 0;

    for (ShotCandidate& candidate : g_shotCandidates) {
        int current = 0;
        if (!ReadValue(candidate.address, current) || current < 0 || current > 500) {
            continue;
        }

        if (current < candidate.initialValue) {
            candidate.initialValue = current;
            candidate.lastValue = current;
            continue;
        }

        const int frameDelta = current - candidate.lastValue;
        if (frameDelta >= 0 && frameDelta <= 10) {
            const int totalDelta = current - candidate.initialValue;
            if (totalDelta > bestDelta) {
                bestDelta = totalDelta;
                bestAddress = candidate.address;
            }
        }
        candidate.lastValue = current;
    }

    if (bestDelta > g_dynamicShotValue) {
        g_dynamicShotValue = bestDelta;
        Log::Write("Dynamic shots counter advanced to %d using 0x%08X", g_dynamicShotValue, static_cast<unsigned>(bestAddress));
    }

    value = g_dynamicShotValue;
    return true;
}

} // namespace

void ReadSnapshot(StatsSnapshot& snapshot) {
    snapshot = StatsSnapshot{};

    const uintptr_t base = GameBase();

    uint64_t rawMissionKey = 0;
    const int missionOffsets[] = {0x98, 0xBC7};
    if (!ReadPointerValue(base + 0x2A6C5C,
                          missionOffsets,
                          sizeof(missionOffsets) / sizeof(missionOffsets[0]),
                          rawMissionKey)) {
        LogReadFailure("mission key");
        return;
    }

    memcpy(snapshot.missionKey, &rawMissionKey, 8);
    snapshot.missionKey[8] = '\0';

    if (IsAllZeroKey(snapshot.missionKey)) {
        return;
    }

    const MissionInfo* mission = FindMission(snapshot.missionKey);
    if (mission == nullptr) {
        return;
    }

    snapshot.missionActive = true;
    snapshot.missionNumber = mission->number;
    strcpy_s(snapshot.missionName, mission->name);

    if (memcmp(g_lastMissionKey, snapshot.missionKey, 8) != 0) {
        memcpy(g_lastMissionKey, snapshot.missionKey, 9);
        ResetDynamicShots();
        Log::Write("Mission detected: #%d %s (%s)", snapshot.missionNumber, snapshot.missionName, snapshot.missionKey);
    }

    const int timeOffsets[] = {0x118, 0xB38, 0x8, 0x1084, 0x24};
    if (!ReadPointerValue(base + 0x2A6C58,
                          timeOffsets,
                          sizeof(timeOffsets) / sizeof(timeOffsets[0]),
                          snapshot.missionTime)) {
        LogReadFailure("mission time");
        return;
    }

    if (snapshot.missionTime <= 0) {
        return;
    }

    if (g_lastMissionTime > 0 && snapshot.missionTime < g_lastMissionTime) {
        ResetDynamicShots();
    }
    g_lastMissionTime = snapshot.missionTime;

    bool countersComplete = true;
    if (!TryReadShotsFromMissionState(base, snapshot.counters.shotsFired)) {
        const int shotsOffsets[] = {0xBD, 0x11C7};
        if (!ReadPointerValue(base + 0x39419,
                              shotsOffsets,
                              sizeof(shotsOffsets) / sizeof(shotsOffsets[0]),
                              snapshot.counters.shotsFired)) {
            if (!TryReadDynamicShots(snapshot.counters.shotsFired)) {
                snapshot.counters.shotsFired = 0;
                countersComplete = false;
                LogDegradedRead("shots fired");
            }
        } else {
            int dynamicShots = 0;
            if (TryReadDynamicShots(dynamicShots) && dynamicShots > snapshot.counters.shotsFired) {
                snapshot.counters.shotsFired = dynamicShots;
            }
        }
    }

    countersComplete = ReadCounterOrZero(base, snapshot.missionNumber, 0x220, "close encounters", snapshot.counters.closeEncounters) && countersComplete;
    countersComplete = ReadCounterOrZero(base, snapshot.missionNumber, 0x208, "headshots", snapshot.counters.headshots) && countersComplete;
    countersComplete = ReadCounterOrZero(base, snapshot.missionNumber, 0x21C, "alerts", snapshot.counters.alerts) && countersComplete;
    countersComplete = ReadCounterOrZero(base, snapshot.missionNumber, 0x210, "enemies killed", snapshot.counters.enemiesKilled) && countersComplete;
    countersComplete = ReadCounterOrZero(base, snapshot.missionNumber, 0x20C, "enemies harmed", snapshot.counters.enemiesHarmed) && countersComplete;
    countersComplete = ReadCounterOrZero(base, snapshot.missionNumber, 0x218, "innocents killed", snapshot.counters.innocentsKilled) && countersComplete;
    countersComplete = ReadCounterOrZero(base, snapshot.missionNumber, 0x214, "innocents harmed", snapshot.counters.innocentsHarmed) && countersComplete;

    snapshot.countersComplete = countersComplete;
    snapshot.missionStarted = true;
}

} // namespace h2stats::StatsReader
