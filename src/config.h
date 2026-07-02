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
};

namespace Config {

void Initialize(const char* moduleDirectory);
void ReloadIfChanged();
OverlayConfig Get();
const char* Path();

} // namespace Config
} // namespace h2stats
