# Glide Aholic

A local server for seven client builds.
Author: Karliky / BugCraft

Create characters, explore maps, see other players, chat and travel between maps.
Characters and positions are saved. Combat is experimental.

Join [BugCraft on Discord](https://discord.gg/F9wnQ5Yaw6).

## Components

| Artifact | Source | Purpose |
|---|---|---|
| `glide-server.exe` | `server/` | Handles login, characters, maps, movement, chat and commands. |
| `glide-launcher.exe` | `launcher/` | Checks the game executable, creates a separate folder to run it, starts the game and loads the DLL. Keep it open while playing. |
| `glide-client.dll` | `client/` | The launcher loads this into the game to connect it to Glide and apply the controls and resource handling for its build. |
| `inject32.exe` | `inject32/` | The x64 launcher starts this helper to load the DLL into 32-bit clients. |
| `glide-catalog.exe` | `catalog/` | `complete` adds an import pack to a game installation. `prepare` reads that installation and creates the data files Glide needs. |
| `glide-accounts.exe` | `accounts/` | Creates and lists accounts. Use `--force` to update a password. Stop the server before using it. |
| `glide-common.lib` | `common/` | Shared code for files, archives, encryption and networking, compiled into the programs. |

## Requirements

- Windows x64.
- To compile: Visual Studio 2017 Build Tools, or a later Visual Studio with
  **MSVC v141 x86/x64** and **Windows SDK 10.0.17763.0**. MinHook is included.
- An installation of one of the builds below, with its executable, `Gw2.dat`,
  browser files and matching Glide data package. Build 49284 needs compatible
  Coherent UI components. Builds 167409 and 204489 need their browser files.

Glide does not download game files or import packs. You supply those separately.
The launcher checks the executable's SHA-256 hash. A different executable with the
same build number may be rejected.

## Supported clients

| Build | Build date | Executable | Gliding | Mounts |
|---:|:---:|---|:---:|:---:|
| 14075 | 04-25-2012 | `Gw2.exe` | No | No |
| 14422 | 06-07-2012 | `Gw2.exe` | No | No |
| 14505 | 06-15-2012 | `Gw2.exe` | No | No |
| 14812 | 07-20-2012 | `Gw2.exe` | No | No |
| 49284 | 05-26-2015 | `Gw2.exe` | Yes | No |
| 167409 | 08-23-2024 | `Gw2-64.exe` | Yes | Supported |
| 204489 | 07-22-2026 | `Gw2-64.exe` | Yes | Supported |

Build dates come from executable PE timestamps in UTC (`mm-dd-yyyy`). Use the same
build for the server and every connected client. Each build has its own accounts.
You can visit maps whose resources are present in your game installation.

## Build and run

Run these commands in PowerShell from the repository root. For another build,
change `$build`, `$client` and the executable path in step 5.

### 1. Compile

```powershell
.\build.bat Release x64
```

This builds both Win32 and x64 programs. Keep `bin/Win32/Release/` and
`bin/x64/Release/` together. Skip this step if you have the Windows binary package.

### 2. Prepare local data

```powershell
$build = 204489
$client = 'C:\Games\GW2-204489\Gw2-64.exe'

.\bin\x64\Release\glide-catalog.exe prepare "$client" --build $build --out data
```

This reads the game installation and creates `data/<build>/`. It works offline
and leaves the installation unchanged. Keep that installation available.
Preparation stops if existing output files differ. Continue only after it
succeeds with exit code 0.

<details>
<summary>Add the Glide data package</summary>

If preparation reports a missing Glide data package, supply an import pack for
your build. It must contain `installation.txt` and the files listed in it:

```text
format=1
build=<build-number>
data=<relative-path>|<byte-size>|<lowercase-sha256>
```

Use a new output directory with enough space for a full copy of the game archive:

```powershell
.\bin\x64\Release\glide-catalog.exe complete "$client" --inputs "C:\GW2-inputs\$build" --out "C:\Games\GW2-$build-complete"
```

Set `$client` to the executable in the new copy, then run `prepare` again. If
completion is interrupted, rerun the same command to resume the `.pending` copy.
The source installation stays unchanged. You still need the original game files
for any maps you want to load.

</details>

### 3. Create an account

With the server stopped:

```powershell
.\bin\x64\Release\glide-accounts.exe --build $build --email player@local.test
```

Enter a test password when prompted. Create each account once per build. Accounts
and characters are saved in `state/`. Passwords are stored in plain text. Use a
password you do not use elsewhere.

### 4. Start the server

```powershell
.\bin\x64\Release\glide-server.exe --config server\server.cfg --build $build
```

Leave this terminal open. The server listens on `127.0.0.1:6112` by default.
Change the address and port in `server/server.cfg` if needed.

### 5. Launch the game

Open a second PowerShell terminal in the same project directory:

```powershell
.\bin\x64\Release\glide-launcher.exe 'C:\Games\GW2-204489\Gw2-64.exe' --windowed --server 127.0.0.1:6112
```

Keep the launcher open. Log in with the account you created, then create or select
a character and enter a map. To play with another client, create a second account
before starting the server and launch a second client of the same build.

Walk, jump and try `.gps` in chat. Exit and reconnect to check that your character
and position were saved. Close the clients and press **Ctrl+C** in the server
terminal to stop it. To test another build, repeat steps 2 to 5.

## Commands

Press **Enter** for chat. Use `.help command` for arguments, such as
`.help worldport`. **OK** means available. **X** means unavailable.

| Command | Purpose | 14075 | 14422 | 14505 | 14812 | 49284 | 167409 | 204489 |
|---|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| `.help` | List commands or show help for one command. | OK | OK | OK | OK | OK | OK | OK |
| `.gps` | Show your map and coordinates. | OK | OK | OK | OK | OK | OK | OK |
| `.heading` | Show the direction you are facing. | OK | OK | OK | OK | OK | OK | OK |
| `.status` | Show character and server state. | OK | OK | OK | OK | OK | OK | OK |
| `.maps` | Find maps by name or ID. | OK | OK | OK | OK | OK | OK | OK |
| `.mapinfo` | Show details for a map. | OK | OK | OK | OK | OK | OK | OK |
| `.who` | List online players. | OK | OK | OK | OK | OK | OK | OK |
| `.where` | Show a player's map and coordinates. | OK | OK | OK | OK | OK | OK | OK |
| `.inspect` | Show a player's position and movement state. | OK | OK | OK | OK | OK | OK | OK |
| `.announce` | Send a message to all players. | OK | OK | OK | OK | OK | OK | OK |
| `.worldport` | Travel to a map or coordinates. | OK | OK | OK | OK | OK | OK | OK |
| `.up` | Move vertically. | OK | OK | OK | OK | OK | OK | OK |
| `.forward` / `.fw` | Move forward. | OK | OK | OK | OK | OK | OK | OK |
| `.backward` / `.bw` | Move backward. | OK | OK | OK | OK | OK | OK | OK |
| `.tele` | Travel to another player. | OK | OK | OK | OK | OK | OK | OK |
| `.bring` | Bring a player to your location. | OK | OK | OK | OK | OK | OK | OK |
| `.bringall` | Bring all other players to your location. | OK | OK | OK | OK | OK | OK | OK |
| `.tpall` | Move all players to a map. | OK | OK | OK | OK | OK | OK | OK |
| `.mark` | Save a named location. | OK | OK | OK | OK | OK | OK | OK |
| `.marks` | List your saved locations. | OK | OK | OK | OK | OK | OK | OK |
| `.unmark` | Delete a saved location. | OK | OK | OK | OK | OK | OK | OK |
| `.go` | Travel to a saved location. | OK | OK | OK | OK | OK | OK | OK |
| `.timeofday` | Show or set the shared time of day. | OK | OK | OK | OK | OK | OK | OK |
| `.combat` | Show or toggle experimental combat with other players. | OK | OK | OK | OK | OK | OK | OK |
| `.glider` | Choose a glider from the local catalog. | X | X | X | X | OK | OK | OK |
| `.mount` | Ride a mount from the local catalog. | X | X | X | X | X | OK | OK |
| `.unmount` | Dismount. | X | X | X | X | X | OK | OK |

Glider and mount IDs must exist in the local catalog.

## Troubleshooting

- **Compilation fails:** check that v141 x86/x64 and SDK 10.0.17763.0 are installed.
- **Preparation fails:** check the data package, archive and browser files for
  your build. Fix the error before launching.
- **Executable rejected:** check that its hash matches a profile in
  `builds/executable_profiles.cpp`.
- **Login fails:** check that the account, server and client use the same build.
- **Map fails to load:** check that your installation contains the map's resources.
  A map can appear in the catalog even when some files are missing.

<details>
<summary><strong>Repository files and local folders</strong></summary>

| Entry | Purpose |
|---|---|
| `builds/` | Code and settings for each client build, including executable hashes, function offsets, message formats and required files. |
| `server/server.cfg` | Server address, port, data folder, save folder and log file. |
| `tests/` | Tests for the code and release packages. Includes `glide-check.exe`, built separately from the programs above. |
| `vendor/` | MinHook source and its license notices. |
| `.github/` | GitHub Actions configuration for builds, formatting, static analysis and tests. |
| `GlideAholic.sln`, `*/.vcxproj` | The Visual Studio solution and each component's C++ project file. |
| `glide.props` | Compiler, linker and output folder settings shared by the C++ projects. |
| `build.bat` | Finds MSBuild and compiles the programs for the chosen configuration and architecture. |
| `quality.ps1` | Runs clang-format and Cppcheck. `-Install` downloads these tools. `-FixFormat` formats the code. |
| `.clang-format` | C/C++ formatting rules used by the quality script. |
| `release.ps1` | Checks release inputs and builds matching source and Windows ZIPs. |
| `release-files.txt` | Lists the source files included in release packages. |
| `.gitignore` | Keeps generated output, local game data, accounts and credentials out of Git. |
| `.gitattributes` | Git text-file and line-ending rules. |
| `README.md` | Setup and usage instructions. |
| `LICENSE` | GPL-3.0-or-later license for original project code. |
| `THIRD_PARTY_NOTICES.md` | Licenses and credits for third-party code. |

These folders are created locally and ignored by Git:

| Directory | Contents |
|---|---|
| `bin/` | Compiled programs, DLLs, libraries and debug symbols for each architecture and configuration. |
| `build/` | Temporary build files, code quality reports and test files. |
| `data/` | Catalogs, content and client resources generated by `prepare` for each build. |
| `runtime/` | Game working folders created by the launcher. |
| `state/` | Accounts, characters and saved progress for each build. |
| `dependencies/` | clang-format and Cppcheck installed by `quality.ps1`. |
| `dist/` | Source and Windows ZIPs, checksums and release test records. |

`.git/` stores Git history and repository settings.

</details>

## License

Original project code is [GPL-3.0-or-later](LICENSE).
See [third-party notices](THIRD_PARTY_NOTICES.md).
Guild Wars 2 and its assets belong to their respective owners. This project is
unofficial and is not affiliated with ArenaNet or NCSOFT.
