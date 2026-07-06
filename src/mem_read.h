#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

// Game-agnostic primitives for reading the host process's memory. Both per-game
// readers build their pointer chains on top of these.
namespace h2stats {

// True if [address, address+size) is committed, readable memory. VirtualQuery
// never faults, and (unlike ReadProcessMemory) Wine answers it locally in
// ntdll — no wineserver round-trip.
inline bool IsReadableRange(uintptr_t address, size_t size) {
    if (address == 0 || size == 0) {
        return false;
    }
    uintptr_t cursor = address;
    const uintptr_t end = address + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &info, sizeof(info)) == 0 ||
            info.State != MEM_COMMIT) {
            return false;
        }
        constexpr DWORD readableProtect = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                          PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((info.Protect & readableProtect) == 0 || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
            return false;
        }
        cursor = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}

// Reads sizeof(T) bytes from `address` in the current process; an invalid
// address fails gracefully instead of faulting. This used ReadProcessMemory,
// but under Wine every ReadProcessMemory is a synchronous wineserver
// round-trip even on the own process — a full pointer-chain snapshot (~40
// reads) cost milliseconds of main-thread time per refresh. A VirtualQuery
// guard plus a direct copy stays entirely in-process.
template <typename T>
bool ReadValue(uintptr_t address, T& value) {
    if (!IsReadableRange(address, sizeof(T))) {
        return false;
    }
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return true;
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
