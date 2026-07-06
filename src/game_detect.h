#pragma once

namespace h2stats {

enum class Game {
    Unknown,
    Hitman2,
    Contracts,
};

// Detects which game the plugin was injected into from the host executable
// name. Unrecognized executables return Game::Unknown (callers fall back to the
// Hitman 2 layout).
Game DetectGame();

} // namespace h2stats
