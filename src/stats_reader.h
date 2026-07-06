#pragma once

namespace h2stats {

struct StatCounters {
    int shotsFired = 0;
    int closeEncounters = 0;
    int headshots = 0;
    int alerts = 0;
    int enemiesKilled = 0;
    int enemiesHarmed = 0;
    int innocentsKilled = 0;
    int innocentsHarmed = 0;
};

struct StatsSnapshot {
    bool missionStarted = false;
    // When true, a single close encounter fails Silent Assassin (Contracts'
    // "Asylum Aftermath" is the only mission with this stricter rule).
    bool strictCloseEncounter = false;
    int missionNumber = 0;
    int missionTime = 0;
    char missionKey[9] = {};
    char missionName[64] = {};
    StatCounters counters;
};

namespace StatsReader {

void ReadSnapshot(StatsSnapshot& snapshot);

} // namespace StatsReader
} // namespace h2stats
