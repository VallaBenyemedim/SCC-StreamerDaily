# SCC — Streamer Daily installation

## Supported version

SCC currently supports the Steam build whose executable SHA-256 is:

```text
DAC4C516BF382A4FBE809EA4940F830D54CEF92CC035DB55DF5871E4A694666C
```

The matching `Assembly-CSharp.dll` SHA-256 is:

```text
D237B41BF9DD4835F2C56E980A19F1AAA523036275A0F780286B67DB7A0EB2C7
```

The launcher checks both values. After a game update, wait for a compatible SCC build.

## Release installation

1. Download the latest `SCC-StreamerDaily-<version>.zip` from GitHub Releases.
2. Right-click the ZIP, choose **Extract All**, and keep all files together.
3. Start the game through Steam and load a save. The Mono runtime must be initialized.
4. Run `SCC-StreamerDaily-Aktif-Et.cmd` from the extracted folder.
5. Wait for `ACTIVE: Mono game data and ImGui menu verified` in the launcher window.
6. Press `Insert` in the game to toggle the menu.

Do not move only the DLL: the activation script and launcher are part of the installation.

## Updating SCC

1. Open **System → Completely unload** in the menu, or run:

   ```powershell
   .\Activate-StreamerDaily.ps1 -Unload
   ```

2. Confirm that the launcher reports the copy as removed.
3. Replace the extracted files with the new release.
4. Run the launcher again.

## Build from source

Install the **Desktop development with C++** workload or the standalone Visual Studio 2022 Build Tools with MSVC v143 and a Windows 10/11 SDK. Then run:

```powershell
git clone https://github.com/VallaBenyemedim/SCC-StreamerDaily.git
cd SCC-StreamerDaily
.\Build-StreamerDaily.ps1
.\Activate-StreamerDaily.ps1 -CheckOnly
```

`-CheckOnly` performs read-only compatibility checks. To activate after a successful check:

```powershell
.\Activate-StreamerDaily.ps1
```

## Troubleshooting

- **DLL not found:** run `Build-StreamerDaily.ps1` or use the complete release archive.
- **Version mismatch:** the game updated; do not force-load the old DLL.
- **Mono not loaded:** enter the game's main menu or load a save, then retry.
- **Menu does not appear:** press `Insert` once and check `SCC-StreamerDaily.log` for `Menu rendering active`.
- **Already active:** use the existing menu; the loader intentionally avoids a duplicate copy.
- **Replacing files fails:** cleanly unload SCC first. Do not terminate the game process to replace a loaded DLL.

The project never asks you to disable Windows security software. If a downloaded binary is not trusted in your environment, build it from the published source.

