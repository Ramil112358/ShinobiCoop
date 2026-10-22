# Main menu polish — design

Date: 2026-10-20. Status: approved direction "A · Live Stage" (mockups: `.superpowers/brainstorm/174-1792501802/content/directions.html`, `jutsu-tab.html`).

## Intent

The main menu today is a black box of default grey buttons ([ShinobiMenuWidgets.cpp](../../../Source/ShinobiCoop/Shinobi/UI/ShinobiMenuWidgets.cpp)). Character and outfit are text rows cycled in Settings, maps are text rows ("Host Arena: Camden 1v1"), and the jutsu loadout can only be edited in-game (K). The user wants a menu that looks like a modern game's — picking ideas from Naruto Storm 4 (character in 3D, portrait cards), Shinobi Striker and Battlefield (top tab bar, map cards with the map filling the background, server browser) — but original and fitted to this game. The game's identity is its custom roster (characters built from the user's photos and pets) and outfits, so the pick should be seen live, not as a name.

## Goals

1. A consistent styled frame across all menu screens: top tab bar **PLAY · SHINOBI · JUTSU · SETTINGS**, player (Steam) name and "Online via Steam" / "Steam not detected - LAN mode" on the right. Gamepad bumpers (LB/RB) and Q/E switch tabs; existing gamepad/keyboard focus navigation keeps working on every screen.
2. A live 3D stage in `Lvl_MainMenu` behind the UI: the player's current pick stands there in their outfit, playing a looping idle.
3. **SHINOBI tab** (new roster screen): portrait card per roster character along the bottom, name on the right, outfit ◀ ▶ selector. Changing either updates the 3D stage immediately and saves (same saved values the Settings rows use today: `ShinobiSettings::GetSavedCharacter/GetSavedOutfit`).
4. **PLAY tab**, two sub-views:
   - **Host**: map list on the left, each row a thumbnail + name + CO-OP or PVP chip. The selected map's image fills the screen background. Right panel: map name, a one-line blurb, max players, and a mode toggle showing only modes the map supports (co-op maps: CO-OP; arenas: 1v1 / 2v2). Big **HOST** button bottom-right.
   - **Find Games**: server-browser rows — map thumbnail, host name, map, mode, players x/y, ping. Refresh button. Empty and searching states.
5. **JUTSU tab**: the K Skills screen's function in the new style, reachable from the menu. Left: every known jutsu as icon tiles grouped **Combat / Elements / Summons & Dragons**, an orange dot on equipped ones. Right: selected jutsu's group name as its type line, name, `Description`, chakra cost, cooldown, seal time (`WindUp`). No new per-jutsu data fields. Bottom: the 8-slot bar with the player's real key labels (ShinobiKeyBindings). Same interactions as the K screen: click jutsu then slot, drag to swap, right-click/drag-out to clear. Centre: the 3D stage character (static idle).
6. **SETTINGS tab**: today's settings rows and Shortcuts, restyled. In the main menu, Character and Outfit rows are left out (SHINOBI replaces them). The pause menu's Settings keeps them, so in-game switching (`PC->ShinobiCharacter/ShinobiOutfit`) keeps working.
7. Real images:
   - **Character portraits** rendered in-engine from each roster character's body mesh (default outfit None, idle pose, consistent framing/lighting), saved as textures. A re-runnable editor script produces them, so a new roster entry gets a portrait by re-running it.
   - **Map thumbnails / backgrounds** captured from each real level (a hero shot of the level, not the minimap), saved as textures. Re-runnable script, one camera spot per map.
8. Look: dark, dusk-orange single accent `#ff7a1a`, angled (parallelogram) card and button shapes, italic uppercase headings, translucent dark panels; reuse existing HUD textures (`Content/Shinobi/UI/Hud`) where they fit. Tab changes cross-fade rather than snap.

## Non-goals

- No jutsu preview animation (hand seals, casting) on the stage — static idle only for now. Hand-seal sequence display is also out (not in jutsu data today).
- No changes to the in-game K Skills screen or pause menu beyond what sharing code naturally brings; the pause menu may keep its current look.
- No new game modes, matchmaking, lobby/party screen, or player profile/stats.
- No new roster characters, outfits or maps.

## Anti-goals (failure even if it "works")

- Portraits or thumbnails that don't match the actual models/levels (AI-painted stand-ins).
- Losing gamepad navigation anywhere in the menu.
- Breaking the auto-host/auto-join command-line flags (`-ShinobiAutoHost`, `-ShinobiAutoHostArena=`, `-ShinobiAutoHostFormat=`, `-ShinobiAutoJoin`) or the existing settings/loadout save formats.
- A menu loadout save that the game then rejects or resets on spawn (must pass `ShinobiLoadout::IsValid` against the same known-jutsu list the pawn uses).

## Constraints and facts

- UE 5.8. Menus are built in C++ (`UShinobiMenuWidget` builds its UMG tree in `RebuildWidget`, like the HUD); keep that approach — no Widget Blueprints.
- Unity build merges UI `.cpp` files: anonymous-namespace names must be unique per file.
- Roster: `AShinobiPlayerCharacter::Roster[]` — read it, never hard-code names; it grows (Ramil, Akane, Goro, Hana… and an uncommitted MetaHuman entry may add a 4th field). Outfits: `AShinobiPlayerCharacter::Outfits[]` (None, Hokage, Akatsuki), with per-body fitted copies `OutfitMeshPath(Outfit, Body)` and hide masks `OutfitHidePath` applied as material param `HideMask` (see `ShinobiPlayerCharacter.cpp` ~205-250). The stage must dress the character the same way the pawn does — share that code rather than copy it.
- Visible bodies are `UShinobiBodyComponent`s that copy a hidden Manny driver's pose through an IK retargeter. The stage actor follows the same pattern: hidden Manny driver playing an idle anim, body component wearing the pick, outfit leader-posed.
- Maps: `UShinobiSessionSubsystem::GetMaps()` (`FShinobiMap {DisplayName, Path, bArena}`). Blurb and thumbnail texture are new per-map data; index order must not change (auto-host and saved defaults rely on it).
- Known jutsu: `AShinobiPlayerCharacter::GetKnownJutsu()` reads `StartupAbilities`; in the menu there is no pawn, so read it from the class default object of the player pawn class the game uses. Jutsu data: `UShinobiJutsuAbility` has `DisplayName`, `Icon`, `Description`, `ChakraCost`, `Cooldown`, `WindUp`. Loadout rules/saving: `ShinobiLoadout` (`IsValid`, `Place`, `Swap`, `Clear`, `LoadSaved`, `Save`). In the menu there is no server and no combat, so edits save directly.
- Today's menu has no map art; `Content/Shinobi/UI/Minimaps` exists but is the wrong kind of image.
- Editor scripts run via the existing Python-in-editor workflow (`Scripts/`), like `setup_hero.py` / `setup_hud.py`. The Unreal MCP connection may be down; scripts must also be runnable from the editor's Python console or commandlet.
- Branch: a new branch `feature/main-menu` off `main` (Blink and outfit fitting are merged there). The shared working tree is checked out on `feature/outfit-fitting` with a paused session's uncommitted RamilMH work (MetaHuman roster entry, `ShinobiBodyComponent`, `ShinobiPlayerCharacter`, character/outfit uassets). Leave that work untouched and out of these commits: don't switch branches in the shared tree (the editor locks uassets). Build commits for `feature/main-menu` through a separate index (`GIT_INDEX_FILE` + `commit-tree` + `update-ref`) containing only this feature's files and hunks. The code must compile both with and without the RamilMH changes. *(my call)*
- User preference: test only what changed; full suite only at the end.

## Decided by the user

- Direction A "Live Stage" over a flat 2D ink style.
- Tabs PLAY / SHINOBI / JUTSU / SETTINGS; Character and Outfit move from the main menu's Settings to SHINOBI and stay in the pause menu's Settings.
- Blink and outfit fitting are merged first (done); RamilMH stays uncommitted and ignored.
- JUTSU tab in the main menu, showing each jutsu's description.
- Static idle on the stage; no jutsu/seal preview for now.

## My calls (user can overrule)

- Portraits rendered from meshes and map images captured from levels (not painted/generated art).
- Accent colour `#ff7a1a`, parallelogram shapes, italic caps.
- Jutsu groups Combat / Elements / Summons & Dragons (from the old page layout: basics + movement = Combat, elements = Elements, summons + dragons + Susanoo = Summons & Dragons — Susanoo may sit in Combat if that reads better).
- Q/E plus LB/RB for tab switching; Esc/B from a tab goes back to PLAY, from PLAY asks nothing (Quit stays a button on PLAY).
- Map blurbs written by the builder, one line each.
- All 9 maps stay in the Host list, keeping their order. Test Level and Combat Level go last with a TEST chip, and they also get captured thumbnails. Display order is co-op, then arenas, then test. Rows keep a row-to-map-index table, as `HostRows` does today.
- The jutsu detail panel has no "chargeable" stat, because every jutsu charges through the base class. The type line is the group name.
- `feature/main-menu` off `main`, committed via a separate index so RamilMH's uncommitted work stays out.

## Left to the builder

- The stage set dressing in `Lvl_MainMenu` (camera, lighting, backdrop — e.g. a dusk rooftop from existing London/Fab content) and where the character stands per tab (it may shift left/right so panels don't cover it).
- Which idle animation asset; portrait and thumbnail resolutions and framing; texture asset names/paths (follow `T_` + `/Game/Shinobi/UI/...` conventions).
- How per-map data (blurb, thumbnail) is stored — extend `FShinobiMap` or a parallel table.
- Widget structure and how much of `UShinobiMenuWidget` / `UShinobiSkillsWidget` is reused vs rewritten, as long as the in-game K screen keeps working.
- Fonts (a free, licence-safe font in the project is fine; engine Roboto otherwise).
- Automated tests: extend the existing ones that touch settings, loadout and the menu (`ShinobiSettingsTests`, ability/loadout tests) for any rule that moves; no tests for pure styling.
