#pragma once

#include "stats_reader.h"

#include <cstddef>
#include <cstdint>

namespace h2stats {

// Per-game strategy that knows how to read one game's memory layout into the
// game-agnostic StatsSnapshot. Exactly one is selected at runtime by the
// detected game (see stats_reader.cpp). Implementations live in
// game_hitman2.cpp and game_contracts.cpp.
class GameReader {
public:
    virtual ~GameReader() = default;

    // Fills `snapshot` (already reset by the caller) from the process memory of
    // the module loaded at `moduleBase`.
    virtual void ReadSnapshot(uintptr_t moduleBase, StatsSnapshot& snapshot) = 0;
};

// Factories return process-lifetime singletons; the caller does not own them.
GameReader* CreateHitman2Reader();
GameReader* CreateContractsReader();

// ---- Shared helpers for the per-game readers (game_common.cpp) ----

// One row of a game's level table: `key` is the raw 8-byte level id read from
// memory; `name`/`number` are the human-readable mission name and index.
struct MissionInfo {
    const char key[9];
    const char* name;
    int number;
};

// Returns the matching mission (comparing the first 8 bytes of `key`) or null.
const MissionInfo* FindMission(const char* key, const MissionInfo* table, size_t count);

// True when the 8-byte level id is all zeros (no active mission / menu).
bool IsAllZeroKey(const char* key);

// Throttled diagnostic logging shared by both readers (once per 5s per line).
void LogReadFailure(const char* context);
void LogDegradedRead(const char* context);

// Logs "Mission detected: ..." once whenever the active mission changes.
void LogMissionChange(const StatsSnapshot& snapshot);

} // namespace h2stats
