# PvP arena: 1v1 / 2v2 on four small London maps

## Context

You asked (2026-10-03) for smaller maps cut from four parts of `Lvl_London` (the original stays) and 1v1 and 2v2 arena fights on them, with power-ups (HP, attack boost, speed boost and more).

**You decided:**
- Best of 3 rounds.
- Full kit in fights: all jutsu pages, summons, the dragon mount.
- The host picks mode and map; players fill team slots in join order; AI bots fill empty slots.
- Power-ups: Health, Attack boost, Speed boost, Chakra refill + Shield.

**Assumed (correct me at review):**
- The four parts are the four districts of `Lvl_London`: West End, Camden Town, Westminster, Tower Bridge.
- Each play area is about 300 x 300 m, centred on a landmark (Piccadilly/Leicester Sq, Camden Lock, Parliament Sq + the bridge, the Tower of London + Tower Bridge).
- Maps are new levels built from the same assets, not a runtime clip of `Lvl_London`.
- Bots are melee/kunai ninjas, not full-kit copies.

**What exists today** (read from the code):
- `AShinobiArenaGameMode` is a PvE wave arena. It stays untouched.
- `EShinobiTeam` is `Players` / `Enemies`. Hostility goes through `AShinobiCharacterBase::IsHostileTo` (9 call sites: abilities, AI targeting, AoE, projectiles, lock-on, feedback). `Team` is set in constructors and is not replicated on characters.
- `UShinobiDamageExec` already scales damage from state tags (Susanoo dealt/taken), so boosts and a shield fit there.
- `UShinobiSessionSubsystem` has a fixed map list and `MaxPlayers = 4`; the session carries a map-name setting for Find Games.
- `Lvl_London` is a World Partition level: landscape, always-loaded 128 m chunk `StaticMeshActor`s (G/B/K/W/X, vertices in map space), one scatter actor per 128 m chunk, invisible-wall markers. Built by `Scripts/london/build_london.py`.

## Design

### 1. Maps

- Four new levels: `Lvl_Arena_WestEnd`, `Lvl_Arena_Camden`, `Lvl_Arena_Westminster`, `Lvl_Arena_Tower` under `/Game/Shinobi/Maps/`.
- New `Scripts/london/build_arenas.py` imports the `build_london.py` helpers and runs the same stages per arena with a rectangle filter:
  - terrain stage: landscape, sun, sky, fog, post (same look as London);
  - city stage: only chunk actors whose bounds touch the rectangle;
  - dress stage: only the scatter actors and landmarks inside it;
  - an invisible wall ring on the rectangle edge, tall (about 60 m) because the dragon mount flies; chunks crossing the edge stay as free backdrop behind the wall.
- `Scripts/london/arena_layout.json` holds, per arena: the rectangle (map metres, +X east, +Y south), team spawn points, pickup spots. It is the only place coordinates live.
- Each level carries:
  - `PlayerStart`s tagged `Team:A` / `Team:B` (4 per team, on opposite sides);
  - `TargetPoint`s tagged `Pickup:Health|Attack|Speed|Shield`;
  - world settings default game mode `AShinobiPvpGameMode`;
  - a NavMeshBoundsVolume and `EnsureNavMesh`, so bots can path.
- **Checkpoint:** a top-down `arena_plan.png` of the four rectangles with spawns and pickups is shown to you before any 3D is built (same practice as London's `plan.png`).
- **Running it:** the build needs `SourceArt/World/London` (OSM data, untracked) and the imported London content. The worktree has neither the data nor the Fab packs, so the build runs in the main checkout or with a junction to it.

### 2. Teams

- `EShinobiTeam` gains `TeamA`, `TeamB`. `Players` and `Enemies` keep their meaning, so co-op and the PvE arena are unchanged.
- `AShinobiCharacterBase::Team` becomes replicated (`COND_InitialOnly`), so clients agree on hostility for HUD, lock-on and feedback.
- `IsHostileTo` is unchanged: different team = hostile. Existing damage, AoE, projectile, AI targeting and lock-on code then works for PvP with no further edits.
- Friendly fire is off between teammates (same team is not hostile). Summons and the Susanoo/barriers use the caster's team, as the barrier already does.
- Outlines are set per viewer: teammates get the friendly stencil, opponents the enemy stencil. The static `Team == Enemies` checks in `ShinobiCharacterBase.cpp` and `ShinobiPlayerCharacter.cpp` become "hostile to the local pawn".
- The HUD teammate list shows teammates only; an opponents' list shows names and HP bars for opponents.

### 3. Match mode: `AShinobiPvpGameMode`

URL options: `?game=...ShinobiPvpGameMode?Format=1v1|2v2`. Format sets slots per team (1 or 2).

New replicated state on `AShinobiGameState` (the existing fields stay):
`PvpPhase`, `RoundNumber`, `WinsA`, `WinsB`, `Format`, `PhaseEndTime` (existing), `RoundWinner`.

Flow:

1. **Warmup (15 s, or until all human slots are filled):** players spawn at their team's starts, free to move and test. Humans are assigned A, B, A, B in login order (`PostLogin`).
2. **Bot fill:** at the end of warmup each empty slot gets one `AShinobiBotFighter` on that team.
3. **Countdown (3 s):** everyone placed at spawns, refilled, input blocked.
4. **Fighting (120 s cap):** a round ends when a team has no living members (downed counts as out, and revive is disabled in this mode). On timeout the team with the higher total health fraction wins the round; an exact tie replays the round (no point awarded); a second tie in a row ends the match as a draw.
5. **Round end (5 s banner):** winner shown, then a reset (respawn all, full HP/chakra/stamina, pickups reset, summons and mounts cleared) and the next countdown.
6. **Match end:** first team with 2 round wins. A 10 s result banner, then the same map reloads with the same format for a rematch (`ServerTravel` to the current map).

A human who disconnects mid-match is replaced by a bot at the next round start. All humans leaving ends the match (session ends as today).

The score logic is a plain struct `FShinobiPvpMatch` (no UObject): round and win counting, win/tie decisions, slot assignment. It is unit-tested like `FShinobiEncounter`.

### 4. Bots: `AShinobiBotFighter`

- Subclass of `AShinobiEnemyCharacter`; the existing `AShinobiAIController` already targets the nearest hostile character, so bots attack players and opposing bots.
- Team is set by the mode at spawn (before BeginPlay, via the deferred spawn like `FShinobiEncounter::Spawn`).
- Stats: player-sized HP (taken from the player's max health), the Rogue Ninja melee combo and a kunai throw; no jutsu, no dragon. Name "Bot".
- Not full-kit by design. A smarter bot is a later feature.

### 5. Power-ups: `AShinobiPickup`

- Replicated actor: sphere overlap, a floating orb mesh with a type colour and a text label above it, a bob/rotate animation. Server applies the effect on overlap by a living, non-bot or bot fighter, then hides itself and respawns after 25 s. Round reset respawns all pickups at once.
- Spawned by the mode at each `Pickup:<Type>` target point (the map holds points, the mode owns the logic).
- Effects (numbers are defaults, tuned in playtest):

| Type | Effect |
|---|---|
| Health | +40% of max HP, instant |
| Attack boost | 15 s state tag, outgoing damage x1.5 |
| Speed boost | 12 s state tag, move speed x1.35 |
| Chakra + Shield | Chakra to max, plus 10 s state tag that absorbs the next 60 damage |

- Boost and shield are timed gameplay effects granting state tags. `UShinobiDamageExec` reads them next to the existing Susanoo tags (attack boost on the source, shield on the target). Speed boost scales walk speed through `ShinobiMovementComponent`.
- Per arena: 2 Health, 1 of each other type, laid out mirror-symmetric between the two spawn sides.

### 6. Lobby

- Host Game gains a mode choice: Co-op map (the current list) or Arena. Arena shows the four maps and Format 1v1 / 2v2.
- `FShinobiMap` gains a `bArena` flag; the arena entries open with the PvP game mode URL and chosen format.
- A new session setting `FormatKey` carries "1v1"/"2v2" (or empty for co-op) so Find Games shows the mode.
- `MaxPlayers` stays 4: 1v1 allows 2 humans, 2v2 allows 4.

### 7. HUD

- Score strip at the top: Team A wins, round number, Team B wins, timer.
- Banners: countdown (3, 2, 1, FIGHT), round winner, match result.
- Active boost icons with a remaining-time bar. They use the existing `ShinobiHUDWidget` canvas helpers; no new art is required (coloured text and bars).

## Error handling and edge cases

- Fewer humans than slots: bots fill; no humans present: the mode idles.
- A player falls out of the world (kill Z in the maps): counts as dead for the round.
- Dragon mount in a 300 m arena: allowed, walls are tall; if it proves unfair it can be disabled per mode with one flag.
- A pickup overlapped by two players at once: the server applies it to the first overlap only.

## Testing

- Automation tests in `Source/ShinobiCoop/Tests/`: `FShinobiPvpMatch` (round/win/tie/slot logic), team hostility with `TeamA`/`TeamB`, pickup effects (health clamp, tag grant, shield absorbs exactly 60), bot targeting an opposing player.
- A host+client smoke script (like `smoke_m4.ps1`) on one arena: both join, teams assigned, a round resolves, the score replicates.
- Screenshot tours of all four arenas via `shot_chakra.ps1 -Tour -Map`.
- Playtest checkpoints with you: arena plan (before 3D), one finished map (before the other three), and the full match loop.

## Build order

Each step is a commit on `feature/pvp-arena` and keeps `main`-style behaviour working.

1. Teams: enum, replication, per-viewer outlines, tests.
2. `FShinobiPvpMatch` and `AShinobiPvpGameMode` with rounds, HUD strip and banners (works first on `Lvl_London`/Training Grounds with hand-placed spawns).
3. Pickups and their effects.
4. Bots.
5. Arena maps: `arena_layout.json`, the plan checkpoint, `build_arenas.py`, one map first, then the other three.
6. Lobby: mode and format selection, session setting.

## Out of scope

Ranked/Elo, spectators, voice, kill cam, custom loadouts, bot jutsu, more than 2 teams, and any change to `Lvl_London` or the PvE arena.
