---
name: workbench-probe
description: Drives Arma Reforger Workbench without touching its UI, to compile the addons, run a test, play a world, or screenshot a menu. Use when a change to Mike's Invade and Annex, UI, HALO Jump or the GM Sandbox addon needs checking in the engine, when the user asks to run a Workbench test or probe, or when a screen has to be looked at.
---

# Workbench probe

Nothing in Workbench is clicked. Two scripts do the work:

1. **A plugin** in Enforce Script, run inside Workbench, that calls Workbench's own API and the game's.
2. **A launcher** in PowerShell that starts Workbench on that plugin, waits, and reads back what happened.

Plugins live in the test addon `F:\Mikes-Invade-and-Annex-Tests\Scripts\WorkbenchGame\`, never in a published addon. The launcher is `F:\Mikes-Invade-and-Annex-Tests\tools\Run-WorkbenchProbe.ps1`.

## Before starting Workbench

- **Steam must be running.** The launcher refuses otherwise. Ask the user before starting Steam.
- **Workbench opens real windows on the user's desktop.** If they are in a game or a call, ask first. Run at most one at a time.
- **Never answer a dialog.** A world with a game mode can raise `Script Authorization Required`; the run then stalls right after `Creating player` in `console.log` until a person answers or the timeout kills it. `worlds/GameMaster/GM_Arland.ent` raises it every time. Ask the user before a run on such a world, and say they will have to answer it.
- **Prefer a world that raises none:** `worlds/Showcase/PBR_Vehicles.ent` (no game mode, no AI world, no map). `Worlds/IA_Arland.ent` (an I&A mission, live map) raised none on 2026-10-04.

## Run a plugin

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "F:\Mikes-Invade-and-Annex-Tests\tools\Run-WorkbenchProbe.ps1" -Plugin IA_TransportPilotTest
```

| Parameter | Use |
|---|---|
| `-Plugin` | The plugin class. Its file name is the class name. |
| `-PluginArgs` | The plugin's own arguments as **one string**: `-PluginArgs "-iaDirectorWorld worlds/Showcase/PBR_Vehicles.ent"`. An array arrives comma-joined and the plugin silently falls back to its defaults. |
| `-ExtraAddons` | Addon folders the test addon does not depend on, such as `F:\Mikes-Invade-and-Annex-GM-Sandbox-Exp`. The launcher writes a throwaway project that depends on them. |
| `-ShotDir` | Folder for window captures. See below. |
| `-TimeoutSec` | Default 300. A run that needs longer is usually stuck. |

It prints the exit code, the log folder, script errors and the plugin's lines from `console.log`, and the lines the plugin wrote to its marks file. Exit code 124 is the timeout.

## Read the result

- **The plugin's own marker decides**, for example `[IA][GmSandboxProbe] PASS` or `failures=0`. A clean compile alone does not show the plugin ran.
- `Can't compile "Game" script module` means a script error; the `SCRIPT (E)` lines above it name the file and line. Errors blamed on vanilla files usually follow from the first real one.
- `dependency '...' can't be added` means an addon GUID is missing from `-addonsDir`; the plugin never runs and Workbench never exits.
- Exit `-1073741819` is a crash. After a plugin that opened the map it happens at exit, in vanilla `SCR_MapMarkerWidgetComponent`, after the plugin has already passed; judge by the plugin's marker.
- Workbench rewrites `resourceDatabase.rdb` in every addon it loads. `git restore resourceDatabase.rdb` in each Exp folder before committing.

## Write a plugin

Copy the shape of `IA_GmSandboxProbe.c` (a test) or `IA_GmDirectorMenuProbe.c` (a screen). The parts every one has:

```c
#ifdef WORKBENCH
[WorkbenchPluginAttribute(name: "IA example probe", wbModules: {"ResourceManager"})]
class IA_ExampleProbe : WorkbenchPlugin
{
	override void RunCommandline()
	{
		// ... checks ...
		Workbench.Exit(m_iFailures);   // the launcher reads this as the exit code
	}
}
#endif
```

- **Report through a marks file.** Append lines to `$profile:<Class>.log` and also `Print` them. The tail of `console.log` is lost in a crash; the marks file is not.
- **Play a world:** `Workbench.OpenModule(WorldEditor)`, `SetOpenedResource(world)`, `SwitchToGameMode(false, false)`, then `Sleep(100)` in a loop until `editor.GetApi().IsGameMode()`. The game keeps running while the plugin sleeps, so wait for what you need (the GM sandbox takes about 7 s to start).
- **Leave a world:** `SwitchToEditMode()`, then `Sleep(15000)` before `Workbench.Exit`. Leaving sooner crashes Workbench.
- **Arguments:** `System.GetCLIParam("name", value)` reads `-name value`. It returns false and empties `value` when the flag is absent, so assign the default after testing the return. A value starting with `-` is read as another flag.
- **Private state of a game class:** add a method to that class under `#ifdef WORKBENCH` in its own addon and call it from the plugin (`IA_GmDirectorMenu.ProbeSelect`, `ProbeAim`). A `modded class` cannot reach it.
- Enforce rules apply as everywhere: no ternary, every `new` in a `ref`, at most nine `string.Format` arguments.

## Look at a screen

The engine's screenshot leaves script-drawn UI out, so the launcher captures the Workbench window instead.

1. In the plugin, put the screen in the state to show, write a mark `shot <name>`, and `Sleep(5000)`.
2. Run with `-ShotDir <folder>`. Each `shot` mark saves `<name>_0.png`, `<name>_1.png`: one file per Workbench window. The larger file is the World Editor with the game view.
3. Read the PNG.

Only Workbench's own windows are captured. Never capture the whole screen: it holds whatever else the user has open.

## What this cannot show

- **Real input.** Functions can be called; a mouse click or key press cannot be produced (`SetCursorPosition` is ignored in Workbench). Check a click path by its numbers, then ask the user to click.
- **Multiplayer.** Play mode is one local session. Dedicated-server and joining-client behaviour needs a real server.
- **The user's screen.** The game view in Workbench has its own size and UI scale.

When the next piece of evidence needs one of these, stop and hand the user the smallest test to run. Do not keep adding probes to stand in for it.

## Record what was learned

A compiler error, a crash, or an engine behaviour that contradicted an assumption goes into `.cursor/rules/enfusion-enforce.mdc` before the turn ends, with the log folder as evidence.
