# ShinobiCoop M3 (Enemies + Waves): Lean Plan

**Goal (spec §3 Enemies, Waves, Downed and revive; §4 HUD):**
- 3 enemy types with AI;
- a wave game mode with a boss;
- enemy HP scaling by player count;
- player downed, revive and bleed-out;
- victory, and wave restart on wipe;
- lock-on, moved here from M2.

**Process:** lean, per the user's request. It's built inline in chunks (build + tests after each), with one light check at the end, then the user's playtest. Branch `feature/m3-enemies-waves`.

## Decisions (deviations noted)

| Topic | Decision |
|---|---|
| Enemy data | Numbers live on C++ classes (`AShinobiEnemyCharacter` subclasses), like the jutsu. There's no `UEnemyData` asset. |
| AI | `AShinobiAIController` runs a server-side FSM (Idle → Chase → Attack → Recover; ranged enemies keep ~800 units away). **Deviation:** it steers directly with `AddMovementInput` instead of NavMesh `MoveTo`: `Lvl_ThirdPerson` has no navmesh, and the arena is open ground. Switch to `MoveTo` once M5's arena has a navmesh. |
| Enemy attacks | `GA_EnemyMelee`: a 2-hit combo using the enemy's `MeleeDamage` (25; boss 50). `GA_EnemyKunai`: a projectile, 30 damage, range 1500. Boss fireball: `GA_EnemyFireball` (a `GA_Fireball` with 90 direct damage, no cost, 8 s cooldown). |
| Boss stun | `ShinobiCombat::ApplyStun` scales `GE_Shock`'s duration by the target's `StunMultiplier` (boss 0.33, so 0.5 s). |
| Death (enemies) | `GE_Dead` grants the replicated `State.Dead` tag. Every machine ragdolls the mesh on that tag, and the server destroys the enemy 3 s later. The game mode counts kills. |
| Downed (players) | At 0 HP a player gets `GE_Downed` (`State.Downed`): abilities blocked, no movement, held death pose (`AM_Downed`), 30 s bleed-out, then `GE_Dead`. Downed or dead characters take no damage (the exec skips them). `GA_Revive` (hold **E** / gamepad **Y** within 200 units for 3 s) revives to 30% HP. Dead players respawn at the next wave start. |
| Game mode | `AShinobiArenaGameMode`: waves 4 RN, then 4 RN + 3 KT, then 6 RN + 4 KT, then the boss. A 10 s break between waves, with a full restore at break start. Enemies spawn on a ring 1200 units around the PlayerStart. HP scales as `Base × (1 + 0.5 × (players − 1))`. The boss summons 2 RN once when it first drops below 50%. A wipe gives a 5 s banner, then the wave restarts. Boss death means victory. `AShinobiGameState` replicates the wave, phase, enemies remaining and phase end time. The dummy mode (`AShinobiGameMode`) stays available via `?game=`. |
| HUD | Top-centre: wave, phase and countdown, plus enemies left. Top-left: teammates' names and health. A downed banner with the bleed-out time; revive progress; VICTORY and WAVE RESTARTING banners. Enemies show overhead HP and status text (a shared component with the dummies). |
| Lock-on | **Tab** / **R3** locks the nearest enemy within 35° of the camera and 3000 units. The camera turns to track it, so every aimed jutsu follows. It releases on death, past 3000, or on another press. Client-side camera only: the server aims from the replicated camera. |

## Status (2026-09-26)

Built in three chunks: enemy core (a6f3f17), downed and revive (b2e9f21), then the arena, HUD, lock-on and downed pose (2b0328b). All 49 automated tests pass, and so does the 2-player dummy smoke test. An idle solo run on `Lvl_ThirdPerson` went wave 1 → downed → wipe → wave 1 restart, as designed.

Notes:
- `setup_m3.py` also points `Lvl_Combat` (the startup map) at the arena. It was on the template's `BP_CombatGameMode` before. Actors the template placed in that level stay there.
- Dev aid: `Shinobi.KillEnemies` (host console) kills the current wave.

## Manual playtest

| # | Check |
|---|---|
| 1 | A 10 s countdown, then wave 1 (4 Rogue Ninjas) spawns on a ring and closes in. |
| 2 | Ninjas telegraph (a slowed swing), then hit twice. A dodge timed into the swing triggers Substitution. |
| 3 | Kunai Throwers (wave 2) hold ~800 units away, back off when approached and throw grey kunai. |
| 4 | Killed enemies ragdoll and vanish after 3 s. "Enemies left" counts down; clearing a wave starts a 10 s break with full HP, chakra and stamina. |
| 5 | Two players: enemy HP ×1.5 (the overhead text shows it). |
| 6 | At 0 HP a player lies down (death pose) with a DOWNED banner and a 30 s countdown. They can't move or act and take no damage. |
| 7 | A teammate holds E / Y (without RB) within 2 m for 3 s, sees "Reviving… %", and the downed player is up at 30% HP. Releasing early cancels. |
| 8 | Bleeding out means dead (ragdoll, "Back at the next wave"). They respawn at the next wave start. |
| 9 | Everyone downed or dead: WIPED OUT banner, 5 s, then the same wave restarts with everyone restored. |
| 10 | Wave 4: the boss (big Manny, 2000 HP × scale) melees for 50 and throws a fireball (8 s cooldown) at range. Chidori stuns it only briefly. Below 50% it summons 2 ninjas once. |
| 11 | Killing the boss shows VICTORY. |
| 12 | Tab / R3 locks the camera onto the enemy nearest the crosshair ("[ LOCKED ]"). Aimed jutsu follow it. It releases on the target's death, beyond 30 m, or on another press. |
| 13 | Top-left shows teammates with HP and DOWNED/DEAD; top-centre shows the wave line. |
