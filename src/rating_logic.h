#pragma once

#include "stats_reader.h"

namespace h2stats::RatingLogic {

// Applies the Silent Assassin rules of the game the snapshot was read from.
bool IsSilentAssassin(const StatsSnapshot& snapshot);
bool IsAllZeros(const StatCounters& counters);

} // namespace h2stats::RatingLogic

