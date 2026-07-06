#include "game_reader.h"

#include "mem_read.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

// Hitman 2: Silent Assassin memory layout. Offsets were tested against the
// Steam build with hitman2.exe file/product version 1.0.0.277.
namespace h2stats {
namespace {

// Most counters are read with a 3-level pointer chain whose second offset
// depends on the current mission; all second offsets are indexed by mission.
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

constexpr size_t kMissionCount = sizeof(kMissions) / sizeof(kMissions[0]);

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

class Hitman2Reader : public GameReader {
public:
    void ReadSnapshot(uintptr_t base, StatsSnapshot& snapshot) override {
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

        const MissionInfo* mission = FindMission(snapshot.missionKey, kMissions, kMissionCount);
        if (mission == nullptr) {
            return;
        }

        snapshot.missionNumber = mission->number;
        strcpy_s(snapshot.missionName, mission->name);
        LogMissionChange(snapshot);

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
};

} // namespace

GameReader* CreateHitman2Reader() {
    static Hitman2Reader reader;
    return &reader;
}

} // namespace h2stats
