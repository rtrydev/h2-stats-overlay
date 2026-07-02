#pragma once

#include "stats_reader.h"

namespace h2stats::RatingLogic {

bool IsSilentAssassin(const StatCounters& counters);
bool IsAllZeros(const StatCounters& counters);

} // namespace h2stats::RatingLogic

