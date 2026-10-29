# SCC — Streamer Daily

[Türkçe](README.tr.md) · **English**

SCC is a modern in-game control panel for the Steam version of **Streamer Daily**. It is an x64 internal DLL with a Dear ImGui / DirectX 11 interface, live Mono data access, persistent profiles and a clean unload path.

> This repository is version-locked to Steam build `6248886`. It is intended for educational and single-player use. It is not affiliated with the game developer or publisher and does not include an anti-cheat bypass.

## Menu preview

![SCC overview](docs/images/overview.png)

| Character controls | Career controls |
|---|---|
| ![Character page](docs/images/character.png) | ![Career page](docs/images/career.png) |

| Stream controls | Movement controls |
|---|---|
| ![Stream page](docs/images/stream.png) | ![Movement page](docs/images/movement.png) |

![System and themes](docs/images/system.png)

## Features

- **Dashboard:** live money, followers, subscribers, health, energy, level and connection state.
- **Character:** independently lock health, energy, needs and happiness to configurable targets.
- **Career:** lock or add money; add followers, subscribers, skill points and experience.
- **Streaming:** keep extra viewers above a configurable minimum.
- **Movement:** `0.50x–5.00x` walking-speed multiplier with automatic restoration.
- **Interface:** resizable premium dark UI, animated controls, keyboard navigation and three accent themes.
- **Profiles:** INI persistence, save/reload controls and reduced-motion mode.
- **Clean unload:** removes input/render hooks, restores captured movement speed and unloads the DLL without closing the game.

## Quick installation

1. Open the repository's **Releases** page and download `SCC-StreamerDaily-<version>.zip`.
2. Extract the complete archive to a permanent folder. Do not run it from inside the ZIP.
3. Start Streamer Daily through Steam and load a save.
4. Run `SCC-StreamerDaily-Aktif-Et.cmd`.
5. Press `Insert` to show or hide SCC.
6. Use **System → Save settings** to persist the profile.
7. Use **System → Completely unload** before deleting or replacing the DLL.

The launcher verifies the game executable and `Assembly-CSharp.dll` hashes before loading anything. A different game build is rejected instead of using stale offsets.

For source builds, troubleshooting and clean removal, see [the detailed English installation guide](docs/INSTALLATION.en.md).

## Build from source

Requirements:

- Windows 10/11 x64
- Visual Studio 2022 C++ Build Tools with the MSVC v143 toolset and Windows SDK
- PowerShell 5.1 or newer

Run:

```powershell
.\Build-StreamerDaily.ps1
```

Output:

```text
bin\x64\Release\SCC-StreamerDaily.dll
```

The repository vendors the exact Dear ImGui and MinHook sources required by the project. See [THIRD_PARTY.md](THIRD_PARTY.md) for license information.

## Technical notes

- Unity `2019.4.20`, Mono, x64, DirectX 11
- Data chain: `OP.bu → OyuncuChar → veri → KarakterS`
- Present / ResizeBuffers hooks through MinHook
- Mono updates run separately from the render thread at approximately 30 Hz
- Profile: `bin\x64\Release\config\client.ini`
- Log: `bin\x64\Release\SCC-StreamerDaily.log`

## License

MIT. Third-party components keep their original licenses. Use the project responsibly and only where you have permission.

