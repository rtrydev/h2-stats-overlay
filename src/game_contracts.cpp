#include "game_reader.h"

#include "config.h"
#include "log.h"
#include "mem_read.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace h2stats {
namespace {

// Counters/timer/shots are read through short pointer chains anchored at these
// module-relative globals.
constexpr int kTimerRva = 0x39457C;      // -> [+0x24], float, seconds
constexpr int kShotsRva = 0x3947B0;      // -> [+0xBA0][+0x104][+0x82F]
constexpr int kStatsRva = 0x3947C0;      // -> [+<per-stat offset>]

constexpr int kStatCloseEncounters = 0xB2F;
constexpr int kStatHeadshots = 0xB17;
constexpr int kStatAlerts = 0xB2B;
constexpr int kStatEnemiesKilled = 0xB1F;
constexpr int kStatEnemiesHarmed = 0xB1B;
constexpr int kStatInnocentsKilled = 0xB27;
constexpr int kStatInnocentsHarmed = 0xB23;

// The Contracts mission-name pointer was never pinned down upstream, so the
// level id is searched for across these candidate chains (module-relative
// address + deref offsets), cycling until one resolves to a known level id.
struct PointerChain {
    int rva;
    int offsets[5];
    int count;
};

constexpr PointerChain kMapPointers[] = {
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

constexpr int kMapPointerCount = static_cast<int>(sizeof(kMapPointers) / sizeof(kMapPointers[0]));

constexpr MissionInfo kMissions[] = {
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

constexpr size_t kMissionCount = sizeof(kMissions) / sizeof(kMissions[0]);

void ReadCounter(uintptr_t base, int statOffset, const char* name, int& value) {
    const int offsets[] = {statOffset};
    if (ReadPointerValue(base + kStatsRva, offsets, 1, value)) {
        return;
    }
    value = 0;
    LogDegradedRead(name);
}

// Diagnostic: log a window of the stat block so each counter's true offset can
// be mapped by watching which value changes after a known action. Enabled with
// [Debug] DumpStats=1. Throttled to once per second.
void LogStatDump(uintptr_t base, float rawTimerSeconds) {
    static DWORD lastTick = 0;
    const DWORD now = GetTickCount();
    if (now - lastTick < 1000) {
        return;
    }
    lastTick = now;

    uint32_t structBase = 0;
    if (!ReadValue(base + kStatsRva, structBase) || structBase == 0) {
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

class ContractsReader : public GameReader {
public:
    void ReadSnapshot(uintptr_t base, StatsSnapshot& snapshot) override {
        // The mission-name pointer is unknown, so try one candidate chain per
        // tick. A candidate that resolves to a known level id is kept
        // (mapPointerIndex_ is left untouched); otherwise the search advances.
        const PointerChain& candidate = kMapPointers[mapPointerIndex_];
        uint64_t rawMissionKey = 0;
        if (!ReadPointerValue(base + candidate.rva, candidate.offsets, candidate.count, rawMissionKey)) {
            mapPointerIndex_ = (mapPointerIndex_ + 1) % kMapPointerCount;
            LogReadFailure("mission key");
            return;
        }

        memcpy(snapshot.missionKey, &rawMissionKey, 8);
        snapshot.missionKey[8] = '\0';

        const MissionInfo* mission = FindMission(snapshot.missionKey, kMissions, kMissionCount);
        if (mission == nullptr) {
            // Either between missions or this candidate isn't the level id pointer.
            mapPointerIndex_ = (mapPointerIndex_ + 1) % kMapPointerCount;
            snapshot.missionKey[0] = '\0';
            return;
        }

        snapshot.missionNumber = mission->number;
        strcpy_s(snapshot.missionName, mission->name);
        // "Asylum Aftermath" (#1) fails SA on any close encounter.
        snapshot.strictCloseEncounter = mission->number == 1;
        LogMissionChange(snapshot);

        float missionTime = 0.0f;
        const int timeOffsets[] = {0x24};
        if (!ReadPointerValue(base + kTimerRva, timeOffsets, 1, missionTime)) {
            LogReadFailure("mission time");
            return;
        }

        if (!(missionTime > 0.0f)) {
            return;
        }
        // Contracts stores the timer as seconds (float); the overlay's
        // FormatTimer works in 1/60s ticks like Hitman 2, so scale it up.
        snapshot.missionTime = static_cast<int>(missionTime * 60.0f);

        ReadCounter(base, kStatCloseEncounters, "close encounters", snapshot.counters.closeEncounters);
        ReadCounter(base, kStatHeadshots, "headshots", snapshot.counters.headshots);
        ReadCounter(base, kStatAlerts, "alerts", snapshot.counters.alerts);
        ReadCounter(base, kStatEnemiesKilled, "enemies killed", snapshot.counters.enemiesKilled);
        ReadCounter(base, kStatEnemiesHarmed, "enemies harmed", snapshot.counters.enemiesHarmed);
        ReadCounter(base, kStatInnocentsKilled, "innocents killed", snapshot.counters.innocentsKilled);
        ReadCounter(base, kStatInnocentsHarmed, "innocents harmed", snapshot.counters.innocentsHarmed);

        int shots = 0;
        const int shotOffsets[] = {0xBA0, 0x104, 0x82F};
        if (ReadPointerValue(base + kShotsRva, shotOffsets, sizeof(shotOffsets) / sizeof(shotOffsets[0]), shots) &&
            shots >= 0 && shots < 100000) {
            snapshot.counters.shotsFired = shots;
        } else {
            LogDegradedRead("shots fired");
        }

        if (Config::Get().dumpStats) {
            LogStatDump(base, missionTime);
        }

        snapshot.missionStarted = true;
    }

private:
    int mapPointerIndex_ = 0;
};

} // namespace

GameReader* CreateContractsReader() {
    static ContractsReader reader;
    return &reader;
}

} // namespace h2stats
