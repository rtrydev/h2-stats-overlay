#pragma once

#include <windows.h>

namespace h2stats::Log {

void Initialize(HMODULE module);
void SetEnabled(bool enabled);
void Write(const char* format, ...);
const char* ModuleDirectory();

} // namespace h2stats::Log

