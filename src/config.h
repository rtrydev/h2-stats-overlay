#pragma once

#include <windows.h>

namespace h2stats {

struct OverlayConfig {
    bool enabled = true;
    int offsetX = 34;
    int offsetY = 150;
    float scale = 1.0f;
    int lineSpacing = 18;
    bool showInMenus = false;
    bool showTimer = true;
    bool verbose = true;
    float verboseScale = 0.75f;
    int verboseLineSpacing = 12;
    bool debugLog = true;
    // Diagnostic: dump the Contracts stat-block window to the log (for mapping
    // which memory offset each counter lives at). Off by default.
    bool dumpStats = false;
};

namespace Config {

void Initialize(const char* moduleDirectory);
void ReloadIfChanged();
OverlayConfig Get();
const char* Path();

} // namespace Config
} // namespace h2stats
