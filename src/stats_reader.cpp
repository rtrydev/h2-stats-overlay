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

struct ResolvedGame {
    GameReader* reader = nullptr;
    Game game = Game::Hitman2;
};

// Resolves and caches the reader for the detected game on first use.
const ResolvedGame& Resolve() {
    static ResolvedGame resolved;
    static bool done = false;
    if (done) {
        return resolved;
    }
    done = true;

    switch (DetectGame()) {
    case Game::Contracts:
        resolved.reader = CreateContractsReader();
        resolved.game = Game::Contracts;
        Log::Write("Game detected: Hitman: Contracts");
        break;
    case Game::Hitman2:
        resolved.reader = CreateHitman2Reader();
        resolved.game = Game::Hitman2;
        Log::Write("Game detected: Hitman 2: Silent Assassin");
        break;
    default:
        resolved.reader = CreateHitman2Reader();
        resolved.game = Game::Hitman2;
        Log::Write("Host executable not recognized; using Hitman 2 offsets");
        break;
    }
    return resolved;
}

} // namespace

void ReadSnapshot(StatsSnapshot& snapshot) {
    snapshot = StatsSnapshot{};

    const ResolvedGame& resolved = Resolve();
    snapshot.game = resolved.game;
    if (resolved.reader != nullptr) {
        resolved.reader->ReadSnapshot(GameBase(), snapshot);
    }
}

} // namespace h2stats::StatsReader
