#include "rating_logic.h"

namespace h2stats::RatingLogic {
namespace {

bool EverythingExceptShotsIsZero(const StatCounters& counters) {
    return counters.closeEncounters == 0 &&
           counters.headshots == 0 &&
           counters.alerts == 0 &&
           counters.enemiesKilled == 0 &&
           counters.enemiesHarmed == 0 &&
           counters.innocentsKilled == 0 &&
           counters.innocentsHarmed == 0;
}

} // namespace

bool IsSilentAssassin(const StatCounters& counters) {
    if (counters.alerts > 2) {
        return false;
    }
    if (counters.alerts == 2 && counters.enemiesKilled > 0 && counters.shotsFired > 0) {
        return false;
    }

    if (counters.closeEncounters > 1) {
        return false;
    }
    if (counters.closeEncounters > 0 && counters.alerts > 0) {
        return false;
    }

    if (counters.shotsFired > 3) {
        return false;
    }
    if (counters.shotsFired == 3 && !EverythingExceptShotsIsZero(counters)) {
        return false;
    }

    if (counters.innocentsKilled > 1) {
        return false;
    }
    if (counters.innocentsKilled > 0 && counters.enemiesKilled > 0) {
        return false;
    }

    if (counters.enemiesKilled > 2) {
        return false;
    }
    if (counters.enemiesKilled == 2 && counters.shotsFired > 0) {
        return false;
    }

    return true;
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
