#include "rating_logic.h"

namespace h2stats::RatingLogic {
namespace {

// Hitman 2: Silent Assassin rules:
//   - One alert allowed; two alerts tolerated when no guard was killed or no
//     shot was fired.
//   - One close encounter allowed; an alert plus a close encounter fails.
//   - At most two shots; a third only when every other counter (including
//     headshots) is zero.
//   - One civilian kill allowed; a civilian kill plus a guard kill fails.
//   - One guard kill allowed; two only when no shots were fired.
bool IsSilentAssassinHitman2(const StatCounters& counters) {
    // One alert allowed; two only when no guard was killed or no shot fired.
    if (counters.alerts > 2) {
        return false;
    }
    if (counters.alerts == 2 && counters.enemiesKilled > 0 && counters.shotsFired > 0) {
        return false;
    }

    // One close encounter allowed, but not together with an alert.
    if (counters.closeEncounters > 1) {
        return false;
    }
    if (counters.closeEncounters > 0 && counters.alerts > 0) {
        return false;
    }

    // Two shots allowed; a third only when every other counter is zero.
    if (counters.shotsFired > 3) {
        return false;
    }
    if (counters.shotsFired == 3) {
        const bool everythingElseZero =
            counters.closeEncounters == 0 &&
            counters.headshots == 0 &&
            counters.alerts == 0 &&
            counters.enemiesKilled == 0 &&
            counters.enemiesHarmed == 0 &&
            counters.innocentsKilled == 0 &&
            counters.innocentsHarmed == 0;
        if (!everythingElseZero) {
            return false;
        }
    }

    // One civilian kill allowed, but not together with a guard kill.
    if (counters.innocentsKilled > 1) {
        return false;
    }
    if (counters.innocentsKilled > 0 && counters.enemiesKilled > 0) {
        return false;
    }

    // One guard kill allowed; two only when no shots were fired.
    if (counters.enemiesKilled > 2) {
        return false;
    }
    if (counters.enemiesKilled == 2 && counters.shotsFired > 0) {
        return false;
    }

    return true;
}

// Hitman: Contracts rules:
//   - One alert allowed.
//   - One close encounter allowed; an alert plus a close encounter fails.
//   - At most one shot if an enemy is killed, at most two if an enemy is
//     harmed; unlimited shots if only scenery/targets were hit.
//   - Civilians can never be harmed or killed.
//   - One enemy kill in most cases; two enemies plus one alert also grants SA
//     as long as everything else is zero.
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

    // Enemy kills: one in most cases; two only alongside a single alert and no
    // other trace (e.g. an accident double-kill).
    if (counters.enemiesKilled > 2) {
        return false;
    }
    if (counters.enemiesKilled == 2) {
        const bool onlyAlertAndKills =
            counters.alerts == 1 &&
            counters.shotsFired == 0 &&
            counters.closeEncounters == 0 &&
            counters.headshots == 0 &&
            counters.enemiesHarmed == 0;
        if (!onlyAlertAndKills) {
            return false;
        }
    }

    // Shots only count against the rating when an enemy was hit.
    if (counters.enemiesKilled > 0) {
        if (counters.shotsFired > 1) {
            return false;
        }
    } else if (counters.enemiesHarmed > 0) {
        if (counters.shotsFired > 2) {
            return false;
        }
    }

    return true;
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
