#include "rating_logic.h"

namespace h2stats::RatingLogic {
namespace {

// Hitman 2: Silent Assassin rules, matching the community-tested combination
// table (Forsaken / HitmanSpeedRuns, "Hitman 2 Silent Assassin Rating"):
//   - One alert allowed, never two (the "two alerts if bodies were only found
//     unconscious" folklore is not in the tested table).
//   - One close encounter allowed; an alert plus a close encounter fails.
//   - Aggression budget: 2*shots + headshots + 3*enemy kills + enemy harms +
//     6*civilian kills + 3*civilian harms + close encounters must not exceed
//     6. Unlike Contracts, shots always count, even into thin air.
bool IsSilentAssassinHitman2(const StatCounters& counters) {
    // One alert allowed.
    if (counters.alerts > 1) {
        return false;
    }

    // One close encounter allowed, but not together with an alert.
    if (counters.closeEncounters > 1) {
        return false;
    }
    if (counters.closeEncounters > 0 && counters.alerts > 0) {
        return false;
    }

    const int aggression = 2 * counters.shotsFired +
                           counters.headshots +
                           3 * counters.enemiesKilled +
                           counters.enemiesHarmed +
                           6 * counters.innocentsKilled +
                           3 * counters.innocentsHarmed +
                           counters.closeEncounters;
    return aggression <= 6;
}

// Hitman: Contracts rules, matching the community-tested combination table
// (Forsaken / HitmanSpeedRuns, "Hitman Contracts Silent Assassin Rating"):
//   - Civilians can never be harmed or killed.
//   - One alert allowed.
//   - One close encounter allowed; an alert plus a close encounter fails.
//   - Aggression budget: 2*shots + headshots + 3*kills + harms + close
//     encounters must not exceed 6. Shots and headshots are free while no
//     enemy was killed or harmed and there was no close encounter (bullets
//     into scenery or mission targets don't count; target kills don't
//     increment the enemy counters).
//   - strictCloseEncounter ("Asylum Aftermath"): any close encounter fails.
bool IsSilentAssassinContracts(const StatCounters& counters, bool strictCloseEncounter) {
    // Civilians can never be harmed or killed.
    if (counters.innocentsKilled > 0 || counters.innocentsHarmed > 0) {
        return false;
    }

    // One alert allowed.
    if (counters.alerts > 1) {
        return false;
    }

    // One close encounter allowed, but not together with an alert.
    if (counters.closeEncounters > 1) {
        return false;
    }
    if (counters.closeEncounters > 0 && counters.alerts > 0) {
        return false;
    }
    if (strictCloseEncounter && counters.closeEncounters > 0) {
        return false;
    }

    // Shots and headshots only start counting once an enemy was killed or
    // harmed or a close encounter happened.
    if (counters.enemiesKilled == 0 && counters.enemiesHarmed == 0 &&
        counters.closeEncounters == 0) {
        return true;
    }

    const int aggression = 2 * counters.shotsFired +
                           counters.headshots +
                           3 * counters.enemiesKilled +
                           counters.enemiesHarmed +
                           counters.closeEncounters;
    return aggression <= 6;
}

} // namespace

bool IsSilentAssassin(const StatsSnapshot& snapshot) {
    if (snapshot.game == Game::Contracts) {
        return IsSilentAssassinContracts(snapshot.counters, snapshot.strictCloseEncounter);
    }
    return IsSilentAssassinHitman2(snapshot.counters);
}

bool IsAllZeros(const StatCounters& counters) {
    return counters.shotsFired == 0 &&
           counters.closeEncounters == 0 &&
           counters.headshots == 0 &&
           counters.alerts == 0 &&
           counters.enemiesKilled == 0 &&
           counters.enemiesHarmed == 0 &&
           counters.innocentsKilled == 0 &&
           counters.innocentsHarmed == 0;
}

} // namespace h2stats::RatingLogic
