# H2 Stats Overlay

Native Win32 ASI plugin for Hitman 2: Silent Assassin and Hitman: Contracts. It hooks Direct3D 8, reads live mission counters from game memory, and draws a small in-game overlay for routing and rating checks.

The plugin detects which game it was injected into from the host executable name (`hitman2.exe` vs `HitmanContracts.exe`) and switches to the matching memory layout automatically. An unrecognized executable falls back to the Hitman 2 layout.

## What it shows

- `SA`: green while the current counters still match the plugin's Silent Assassin rule set, red once they do not.
- `AZ`: green while every tracked counter is zero, red after any tracked counter changes.
- Optional mission timer.
- Optional verbose counter list for shots fired, close encounters, headshots, alerts, enemies killed, enemies harmed, innocents killed, and innocents harmed.

The overlay uses a built-in pixel font and does not require external assets.

## Requirements

- Windows.
- Hitman 2: Silent Assassin or Hitman: Contracts using Direct3D 8. Hitman 2 tested with the Steam release, version 1.02. Contracts support is experimental (see Compatibility).
- An ASI loader that loads plugins from the game's `scripts` directory. Tested with Ultimate ASI Loader 9.5.0 installed as `d3d8.dll`.
- Visual Studio 2022 Community or Build Tools with the v143 C++ toolchain.

## Build

From PowerShell:

```powershell
.\build.ps1
```

The script builds a 32-bit ASI plugin and writes it to:

```text
build\Release\h2_stats_overlay.asi
```

The build does not touch the game installation.

### Cross-compiling from macOS or Linux

Use `build-cross.sh` with MinGW-w64 (`brew install mingw-w64`):

```bash
./build-cross.sh
```

It writes the same `build/Release/h2_stats_overlay.asi`. The script passes `-msse2 -mfpmath=sse` on purpose: MSVC compiles 32-bit float math to SSE2, but MinGW defaults to the x87 FPU, which is emulated slowly by translation layers such as CrossOver/Rosetta on macOS and cripples the in-game framerate. Do not drop those flags.

## Install

Copy these files manually into the game's `scripts` directory (for the Steam release, `<Steam>\steamapps\common\Hitman 2 Silent Assassin\scripts`):

```text
h2_stats_overlay.asi
h2_stats_overlay.ini
```

The game must be closed while replacing the ASI, otherwise the copy fails because the file is loaded.

The plugin also creates `h2_stats_overlay.log` beside the ASI when debug logging is enabled.

## Configuration

The configuration file is `h2_stats_overlay.ini` beside the ASI. If it is missing, the plugin creates a default one. Changes are reloaded while the game is running.

```ini
[Overlay]
Enabled=1
OffsetX=34
OffsetY=150
Scale=1.0
LineSpacing=18
ShowInMenus=0
ShowTimer=1
Verbose=1
VerboseScale=0.75
VerboseLineSpacing=12

[Debug]
Log=1
```

`OffsetX` and `OffsetY` move the overlay relative to the viewport. `Scale` and `LineSpacing` control the `SA`/`AZ` labels. `ShowTimer`, `Verbose`, `VerboseScale`, and `VerboseLineSpacing` control the detailed mission readout. `ShowInMenus` allows the overlay to render outside active missions.

## Compatibility

The Hitman 2 memory offsets were tested against the installed Steam copy of Hitman 2: Silent Assassin, app ID 6850, build ID 251795, with `hitman2.exe` file/product version `1, 0, 0, 277`. The installed ASI loader was Ultimate ASI Loader 9.5.0. Other game versions, executables, or wrappers may require offset updates.

The Hitman: Contracts offsets target the Steam release with `HitmanContracts.exe` file/product version `1, 0, 0, 175` (in-game build 175) and are **experimental / unverified**. In particular, the mission-name pointer is resolved by cycling through eleven candidate pointer chains until one resolves to a known level id (see `kContractsMapPointers` in `src/stats_reader.cpp`). If nothing appears in a mission, or the counters are wrong, the offsets likely need re-finding against your `HitmanContracts.exe` build. Enable `Verbose=1` and `[Debug] Log=1` to watch mission detection in `h2_stats_overlay.log`.

To install for Contracts, copy `h2_stats_overlay.asi` and `h2_stats_overlay.ini` into the Contracts `scripts` directory instead (for the Steam release, `<Steam>\steamapps\common\Hitman Contracts\scripts`).
