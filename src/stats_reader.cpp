#include "stats_reader.h"

#include "config.h"
#include "log.h"

#include <windows.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace h2stats::StatsReader {
namespace {

enum class Game { Unknown, Hitman2, Contracts };

constexpr uintptr_t kPreferredBaseAddress = 0x00400000;

constexpr int kSecondOffsets[] = {
    0x838, 0xB24, 0x8A0, 0x138, 0xB88,
    0xBB8, 0xB48, 0xCE8, 0x136C, 0xAD0,
    0xF50, 0x8D4, 0x9EC, 0x400, 0x9EC,
    0x644, 0xB08, 0x96C, 0xB00, 0x8
};

// Offsets inside the per-mission stats struct (resolved via base+0x2A6C50).
constexpr int kStatHeadshots = 0x208;
constexpr int kStatEnemiesHarmed = 0x20C;
constexpr int kStatEnemiesKilled = 0x210;
constexpr int kStatInnocentsHarmed = 0x214;
constexpr int kStatInnocentsKilled = 0x218;
constexpr int kStatAlerts = 0x21C;
constexpr int kStatCloseEncounters = 0x220;
constexpr int kStatPlayerEntityId = 0x94;

// Offsets inside the player entity. The statistics screen (statisticsscreen.cpp)
// reads shots fired from [player+0x11C7] and adds [player+0x11CB] to the
// mission struct's close encounter counter. Both values are serialized with
// the player entity, so they survive save loads.
constexpr int kPlayerShotsFired = 0x11C7;
constexpr int kPlayerCloseEncounters = 0x11CB;

// Contracts reads most counters through short pointer chains anchored at these
// module-relative globals.
constexpr int kHcTimerRva = 0x39457C;      // -> [+0x24], float, 1/60s units
constexpr int kHcShotsRva = 0x3947B0;      // -> [+0xBA0][+0x104][+0x82F]
constexpr int kHcStatsRva = 0x3947C0;      // -> [+<per-stat offset>]

constexpr int kHcCloseEncounters = 0xB2F;
constexpr int kHcHeadshots = 0xB17;
constexpr int kHcAlerts = 0xB2B;
constexpr int kHcEnemiesKilled = 0xB1F;
constexpr int kHcEnemiesHarmed = 0xB1B;
constexpr int kHcInnocentsKilled = 0xB27;
constexpr int kHcInnocentsHarmed = 0xB23;

struct MissionInfo {
    const char key[9];
    const char* name;
    int number;
};

// The Contracts mission-name pointer was never pinned down upstream, so the
// level id is searched for across these candidate chains (module-relative
// address + deref offsets), cycling until one resolves to a known level id.
struct HcPointerChain {
    int rva;
    int offsets[5];
    int count;
};

constexpr HcPointerChain kContractsMapPointers[] = {
    {0x393D58, {0x234, 0xBDE}, 2},
    {0x394598, {0x10, 0x194, 0xC0E}, 3},
    {0x394598, {0x214, 0xC0E}, 2},
    {0x394578, {0x1EC0, 0x49FA}, 2},
    {0x394578, {0x1E00, 0xBC, 0x49FA}, 3},
    {0x394578, {0x1D80, 0x7C, 0xBC, 0x49FA}, 4},
    {0x394578, {0x1D00, 0x7C, 0x7C, 0xBC, 0x49FA}, 5},
    {0x39457C, {0x1E40, 0x49FA}, 2},
    {0x39457C, {0x1D80, 0xBC, 0x49FA}, 3},
    {0x39457C, {0x1D00, 0x7C, 0xBC, 0x49FA}, 4},
    {0x39457C, {0x1C80, 0x7C, 0x7C, 0xBC, 0x49FA}, 5},
};

constexpr int kContractsMapPointerCount =
    static_cast<int>(sizeof(kContractsMapPointers) / sizeof(kContractsMapPointers[0]));

constexpr MissionInfo kMissionsContracts[] = {
    {"C01-1_MA", "Asylum Aftermath", 1},
    {"C01-2_MA", "The Meat King's Party", 2},
    {"C02-1_MA", "The Bjarkhov Bomb", 3},
    {"C03-1_MA", "Beldingford Manor", 4},
    {"C06-1_MA", "Rendezvous in Rotterdam", 5},
    {"C06-2_MA", "Deadly Cargo", 6},
    {"C07-1_MA", "Traditions of the Trade", 7},
    {"C08-1_MA", "Slaying a Dragon", 8},
    {"C08-2_MA", "The Wang Fou Incident", 9},
    {"C08-3_MA", "The Seafood Massacre", 10},
    {"C08-4_MA", "Lee Hong Assassination", 11},
    {"C09-1_MA", "Hunter and Hunted", 12},
};

constexpr MissionInfo kMissionsHitman2[] = {
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

Game g_game = Game::Unknown;
bool g_gameResolved = false;
int g_hcPointerIndex = 0;

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

const MissionInfo* FindMission(const char* key, const MissionInfo* table, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (memcmp(key, table[index].key, 8) == 0) {
            return &table[index];
        }
    }
    return nullptr;
}

// Detects which game the plugin was injected into from the host executable
// name. Cached after the first call; unrecognized executables fall back to the
// original Hitman 2 behaviour.
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

Game ResolveGame() {
    if (!g_gameResolved) {
        g_gameResolved = true;
        g_game = DetectGame();
        switch (g_game) {
        case Game::Contracts:
            Log::Write("Game detected: Hitman: Contracts");
            break;
        case Game::Hitman2:
            Log::Write("Game detected: Hitman 2: Silent Assassin");
            break;
        default:
            Log::Write("Host executable not recognized; using Hitman 2 offsets");
            break;
        }
    }
    return g_game;
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

void ReadCounterOrZero(uintptr_t base, int missionNumber, int statOffset, const char* name, int& value) {
    if (ReadCounter(base, missionNumber, statOffset, value)) {
        return;
    }

    value = 0;
    LogDegradedRead(name);
}

// Resolves the player entity through the game's entity registry, the same way
// the statistics screen does: the mission stats struct stores the player
// entity id at +0x94, which is looked up in the registry at [[base+0x2A6C5C]+0x98].
bool ResolvePlayerEntity(uintptr_t base, int missionNumber, uintptr_t& entity) {
    uint32_t playerId = 0;
    if (!ReadCounter(base, missionNumber, kStatPlayerEntityId, reinterpret_cast<int&>(playerId)) || playerId == 0) {
        return false;
    }

    // registry = [[base+0x2A6C5C]+0x98], inner = [registry+0x4]
    const int innerOffsets[] = {0x98, 0x04};
    uintptr_t innerAddress = 0;
    if (!ResolvePointerAddress(base + 0x2A6C5C, innerOffsets, 2, innerAddress)) {
        return false;
    }
    uint32_t inner = 0;
    if (!ReadValue(innerAddress, inner) || inner == 0) {
        return false;
    }

    uint32_t node = 0;
    if ((playerId & 0x40000000u) != 0) {
        // Pool branch: node = [[inner+0x14]+0x4] + (id & 0x3FFFFFFF)
        uint32_t pool = 0;
        if (!ReadValue(static_cast<uintptr_t>(inner) + 0x14, pool) || pool == 0) {
            return false;
        }
        uint32_t poolBase = 0;
        if (!ReadValue(static_cast<uintptr_t>(pool) + 0x04, poolBase) || poolBase == 0) {
            return false;
        }
        node = poolBase + (playerId & 0x3FFFFFFFu);
    } else {
        // Hash branch: node = [[inner+0xC] + (id & [[inner+0x8]+0x10]) * 4]
        uint32_t hashInfo = 0;
        if (!ReadValue(static_cast<uintptr_t>(inner) + 0x08, hashInfo) || hashInfo == 0) {
            return false;
        }
        uint32_t mask = 0;
        if (!ReadValue(static_cast<uintptr_t>(hashInfo) + 0x10, mask)) {
            return false;
        }
        uint32_t table = 0;
        if (!ReadValue(static_cast<uintptr_t>(inner) + 0x0C, table) || table == 0) {
            return false;
        }
        if (!ReadValue(static_cast<uintptr_t>(table) + static_cast<uintptr_t>(playerId & mask) * 4, node) || node == 0) {
            return false;
        }
    }

    uint32_t flags = 0;
    if (!ReadValue(static_cast<uintptr_t>(node) + 0x3C, flags) || (flags & 0x4000000u) != 0) {
        return false;
    }

    uint32_t entityPointer = 0;
    if (!ReadValue(static_cast<uintptr_t>(node) + 0x54, entityPointer) || entityPointer == 0) {
        return false;
    }

    entity = entityPointer;
    return true;
}

void ReadSnapshotHitman2(StatsSnapshot& snapshot, uintptr_t base) {
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

    const MissionInfo* mission = FindMission(snapshot.missionKey, kMissionsHitman2,
                                             sizeof(kMissionsHitman2) / sizeof(kMissionsHitman2[0]));
    if (mission == nullptr) {
        return;
    }

    snapshot.missionNumber = mission->number;
    strcpy_s(snapshot.missionName, mission->name);

    if (memcmp(g_lastMissionKey, snapshot.missionKey, 8) != 0) {
        memcpy(g_lastMissionKey, snapshot.missionKey, 9);
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

    ReadCounterOrZero(base, snapshot.missionNumber, kStatCloseEncounters, "close encounters", snapshot.counters.closeEncounters);
    ReadCounterOrZero(base, snapshot.missionNumber, kStatHeadshots, "headshots", snapshot.counters.headshots);
    ReadCounterOrZero(base, snapshot.missionNumber, kStatAlerts, "alerts", snapshot.counters.alerts);
    ReadCounterOrZero(base, snapshot.missionNumber, kStatEnemiesKilled, "enemies killed", snapshot.counters.enemiesKilled);
    ReadCounterOrZero(base, snapshot.missionNumber, kStatEnemiesHarmed, "enemies harmed", snapshot.counters.enemiesHarmed);
    ReadCounterOrZero(base, snapshot.missionNumber, kStatInnocentsKilled, "innocents killed", snapshot.counters.innocentsKilled);
    ReadCounterOrZero(base, snapshot.missionNumber, kStatInnocentsHarmed, "innocents harmed", snapshot.counters.innocentsHarmed);

    uintptr_t player = 0;
    if (ResolvePlayerEntity(base, snapshot.missionNumber, player)) {
        int shots = 0;
        if (ReadValue(player + kPlayerShotsFired, shots) && shots >= 0 && shots < 100000) {
            snapshot.counters.shotsFired = shots;
        } else {
            LogDegradedRead("shots fired");
        }

        int closeEncounterExtra = 0;
        if (ReadValue(player + kPlayerCloseEncounters, closeEncounterExtra) &&
            closeEncounterExtra >= 0 && closeEncounterExtra < 100000) {
            snapshot.counters.closeEncounters += closeEncounterExtra;
        }
    } else {
        LogDegradedRead("player entity");
    }

    snapshot.missionStarted = true;
}

void ReadHcCounter(uintptr_t base, int statOffset, const char* name, int& value) {
    const int offsets[] = {statOffset};
    if (ReadPointerValue(base + kHcStatsRva, offsets, 1, value)) {
        return;
    }
    value = 0;
    LogDegradedRead(name);
}

// Diagnostic: log a window of the Contracts stat block so each counter's true
// offset can be mapped by watching which value changes after a known action.
// Enabled with [Debug] DumpStats=1. Throttled to once per second.
void LogContractsStatDump(uintptr_t base, float rawTimerSeconds) {
    static DWORD lastTick = 0;
    const DWORD now = GetTickCount();
    if (now - lastTick < 1000) {
        return;
    }
    lastTick = now;

    uint32_t structBase = 0;
    if (!ReadValue(base + kHcStatsRva, structBase) || structBase == 0) {
        return;
    }

    char line[384];
    int pos = snprintf(line, sizeof(line), "HC stat dump timer=%.3f [+0x3947C0]->%08X:",
                       rawTimerSeconds, structBase);
    for (int off = 0xB03; off <= 0xB4F && pos > 0 && pos < static_cast<int>(sizeof(line)) - 20; off += 4) {
        int value = 0;
        ReadValue(static_cast<uintptr_t>(structBase) + static_cast<uintptr_t>(off), value);
        pos += snprintf(line + pos, sizeof(line) - pos, " %X=%d", off, value);
    }
    Log::Write("%s", line);
}

void ReadSnapshotContracts(StatsSnapshot& snapshot, uintptr_t base) {
    // The mission-name pointer is unknown, so try one candidate chain per tick.
    // A candidate that resolves to a known level id is kept (g_hcPointerIndex is
    // left untouched); otherwise the search advances to the next candidate.
    const HcPointerChain& candidate = kContractsMapPointers[g_hcPointerIndex];
    uint64_t rawMissionKey = 0;
    if (!ReadPointerValue(base + candidate.rva, candidate.offsets, candidate.count, rawMissionKey)) {
        g_hcPointerIndex = (g_hcPointerIndex + 1) % kContractsMapPointerCount;
        LogReadFailure("mission key");
        return;
    }

    memcpy(snapshot.missionKey, &rawMissionKey, 8);
    snapshot.missionKey[8] = '\0';

    const MissionInfo* mission = FindMission(snapshot.missionKey, kMissionsContracts,
                                             sizeof(kMissionsContracts) / sizeof(kMissionsContracts[0]));
    if (mission == nullptr) {
        // Either between missions or this candidate isn't the level id pointer.
        g_hcPointerIndex = (g_hcPointerIndex + 1) % kContractsMapPointerCount;
        snapshot.missionKey[0] = '\0';
        return;
    }

    snapshot.missionNumber = mission->number;
    strcpy_s(snapshot.missionName, mission->name);
    // "Asylum Aftermath" (#1) fails SA on any close encounter.
    snapshot.strictCloseEncounter = mission->number == 1;

    if (memcmp(g_lastMissionKey, snapshot.missionKey, 8) != 0) {
        memcpy(g_lastMissionKey, snapshot.missionKey, 9);
        Log::Write("Mission detected: #%d %s (%s)", snapshot.missionNumber, snapshot.missionName, snapshot.missionKey);
    }

    float missionTime = 0.0f;
    const int timeOffsets[] = {0x24};
    if (!ReadPointerValue(base + kHcTimerRva, timeOffsets, 1, missionTime)) {
        LogReadFailure("mission time");
        return;
    }

    if (!(missionTime > 0.0f)) {
        return;
    }
    // Contracts stores the mission timer as seconds (float); the overlay's
    // FormatTimer works in 1/60s ticks like Hitman 2, so scale it up.
    snapshot.missionTime = static_cast<int>(missionTime * 60.0f);

    ReadHcCounter(base, kHcCloseEncounters, "close encounters", snapshot.counters.closeEncounters);
    ReadHcCounter(base, kHcHeadshots, "headshots", snapshot.counters.headshots);
    ReadHcCounter(base, kHcAlerts, "alerts", snapshot.counters.alerts);
    ReadHcCounter(base, kHcEnemiesKilled, "enemies killed", snapshot.counters.enemiesKilled);
    ReadHcCounter(base, kHcEnemiesHarmed, "enemies harmed", snapshot.counters.enemiesHarmed);
    ReadHcCounter(base, kHcInnocentsKilled, "innocents killed", snapshot.counters.innocentsKilled);
    ReadHcCounter(base, kHcInnocentsHarmed, "innocents harmed", snapshot.counters.innocentsHarmed);

    int shots = 0;
    const int shotOffsets[] = {0xBA0, 0x104, 0x82F};
    if (ReadPointerValue(base + kHcShotsRva, shotOffsets, sizeof(shotOffsets) / sizeof(shotOffsets[0]), shots) &&
        shots >= 0 && shots < 100000) {
        snapshot.counters.shotsFired = shots;
    } else {
        LogDegradedRead("shots fired");
    }

    if (Config::Get().dumpStats) {
        LogContractsStatDump(base, missionTime);
    }

    snapshot.missionStarted = true;
}

} // namespace

void ReadSnapshot(StatsSnapshot& snapshot) {
    snapshot = StatsSnapshot{};

    const uintptr_t base = GameBase();

    switch (ResolveGame()) {
    case Game::Contracts:
        ReadSnapshotContracts(snapshot, base);
        break;
    case Game::Hitman2:
    default:
        ReadSnapshotHitman2(snapshot, base);
        break;
    }
}

} // namespace h2stats::StatsReader
