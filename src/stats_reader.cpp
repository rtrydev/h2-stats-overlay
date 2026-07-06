#include "stats_reader.h"

#include "game_detect.h"
#include "game_reader.h"
#include "log.h"

#include <windows.h>

#include <cstdint>

// Game-agnostic entry point. Detects the host game once, selects the matching
// per-game reader (game_hitman2.cpp / game_contracts.cpp), and dispatches to it.
namespace h2stats::StatsReader {
namespace {

constexpr uintptr_t kPreferredBaseAddress = 0x00400000;

uintptr_t GameBase() {
    const HMODULE exe = GetModuleHandleA(nullptr);
    if (exe == nullptr) {
        return kPreferredBaseAddress;
    }
    return reinterpret_cast<uintptr_t>(exe);
}

// Resolves and caches the reader for the detected game on first use.
GameReader* ResolveReader() {
    static GameReader* reader = nullptr;
    static bool resolved = false;
    if (resolved) {
        return reader;
    }
    resolved = true;

    switch (DetectGame()) {
    case Game::Contracts:
        reader = CreateContractsReader();
        Log::Write("Game detected: Hitman: Contracts");
        break;
    case Game::Hitman2:
        reader = CreateHitman2Reader();
        Log::Write("Game detected: Hitman 2: Silent Assassin");
        break;
    default:
        reader = CreateHitman2Reader();
        Log::Write("Host executable not recognized; using Hitman 2 offsets");
        break;
    }
    return reader;
}

} // namespace

void ReadSnapshot(StatsSnapshot& snapshot) {
    snapshot = StatsSnapshot{};

    GameReader* reader = ResolveReader();
    if (reader != nullptr) {
        reader->ReadSnapshot(GameBase(), snapshot);
    }
}

} // namespace h2stats::StatsReader
