#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

// Game-agnostic primitives for reading the host process's memory. Both per-game
// readers build their pointer chains on top of these.
namespace h2stats {

// Reads sizeof(T) bytes from `address` in the current process. Uses
// ReadProcessMemory so an invalid address fails gracefully instead of faulting.
template <typename T>
bool ReadValue(uintptr_t address, T& value) {
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(GetCurrentProcess(),
                             reinterpret_cast<LPCVOID>(address),
                             &value,
                             sizeof(T),
                             &bytesRead) != FALSE &&
           bytesRead == sizeof(T);
}

// Walks a pointer chain: read a uint32 at `address`, add offsets[0], and repeat
// for each offset. Returns the final address (not yet dereferenced). Fails if
// any link reads as null or cannot be read.
inline bool ResolvePointerAddress(uintptr_t address, const int* offsets, size_t offsetCount, uintptr_t& resolvedAddress) {
    uintptr_t current = address;
    for (size_t index = 0; index < offsetCount; ++index) {
        uint32_t next = 0;
        if (!ReadValue(current, next) || next == 0) {
            return false;
        }
        current = static_cast<uintptr_t>(next) + static_cast<uintptr_t>(offsets[index]);
    }

    resolvedAddress = current;
    return true;
}

// Resolves a pointer chain and reads a value of type T from the final address.
template <typename T>
bool ReadPointerValue(uintptr_t address, const int* offsets, size_t offsetCount, T& value) {
    uintptr_t resolvedAddress = 0;
    if (!ResolvePointerAddress(address, offsets, offsetCount, resolvedAddress)) {
        return false;
    }

    return ReadValue(resolvedAddress, value);
}

} // namespace h2stats
