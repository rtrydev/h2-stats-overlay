# H2 Stats Overlay

Native Win32 ASI plugin for Hitman 2: Silent Assassin. It hooks Direct3D 8, reads live mission counters from game memory, and draws a small in-game overlay for routing and rating checks.

## What it shows

- `SA`: green while the current counters still match the plugin's Silent Assassin rule set, red once they do not.
- `AZ`: green while every tracked counter is zero, red after any tracked counter changes.
- Optional mission timer.
- Optional verbose counter list for shots fired, close encounters, headshots, alerts, enemies killed, enemies harmed, innocents killed, and innocents harmed.

The overlay uses a built-in pixel font and does not require external assets.

## Requirements

- Windows.
- Hitman 2: Silent Assassin using Direct3D 8. Tested with the Steam release, version 1.02.
- An ASI loader that loads plugins from the game's `scripts` directory. Tested with Ultimate ASI Loader 9.5.0 installed as `d3d8.dll`.
- Visual Studio 2022 Community or Build Tools with the v143 C++ toolchain.

## Build

From PowerShell:

```powershell
.\build.ps1
```

By default, the script builds a 32-bit ASI plugin and installs it to:

```text
..\..\Hitman 2 Silent Assassin\scripts\h2_stats_overlay.asi
```

To build without installing:

```powershell
.\build.ps1 -NoInstall
```

The local build output is written to:

```text
build\Release\h2_stats_overlay.asi
```

## Install

If you do not use the automatic install step, copy these files into the game's `scripts` directory:

```text
h2_stats_overlay.asi
h2_stats_overlay.ini
```

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

The current memory offsets were tested against the installed Steam copy of Hitman 2: Silent Assassin, app ID 6850, build ID 251795, with `hitman2.exe` file/product version `1, 0, 0, 277`. The installed ASI loader was Ultimate ASI Loader 9.5.0. Other game versions, executables, or wrappers may require offset updates.
