# ShinobiCoop — Sub-project #1: Core Combat Slice — Design

Date: 2026-09-23
Status: Approved in brainstorming, pending written-spec review

## 1. Context

ShinobiCoop is a 3D open-world co-op action game where players use Naruto-style jutsu. The full vision is split into four sub-projects, each with its own spec, plan and build:

1. **Core combat slice** (this document)
2. Photo → character pipeline (user's photos → A-pose reference → image-to-3D → Blender cleanup/rig → UE retarget)
3. Open world (landscape, village, forest, camps, NPCs)
4. Progression (XP, levels, jutsu unlock tree, quests, bosses)

The slice proves combat is fun and co-op networking works before investing in art and world.

### Decisions made

| Topic | Decision |
|---|---|
| Co-op | Online via Steam, listen server (host also plays), 2–4 players, tested with 2 |
| Combat style | Third-person action RPG: free camera, crosshair aim, soft auto-target, optional lock-on |
| Casting | Hotkey + hand-seal wind-up; interruptible |
| Art style | Anime cel-shaded (post-process toon + outlines) |
| Architecture | C++ + Gameplay Ability System (GAS) |
| Engine / tools | Unreal Engine 5.8, Blender 5.2, RTX 5080, 32 GB RAM |
| Project path | `C:\Users\ahmad\ShinobiCoop` (not OneDrive-synced Documents) |

### Research: features borrowed from similar games

| Game | Borrowed feature |
|---|---|
| Naruto to Boruto: Shinobi Striker | Chakra dash, 4-player co-op missions |
| Naruto Ultimate Ninja Storm 4 / Connections | Chakra charge (hold to refill, vulnerable); substitution jutsu with limited charges |
| Genshin Impact | Element reactions rewarding teamwork (apply status + detonate); stamina for sprint/dash |
| Elden Ring / Helldivers 2 | Downed state + teammate revive; enemy scaling with player count |
| Hades / Diablo | Hit-stop, camera shake, floating damage numbers |
| Shinobi Life 2 | Chakra as core resource, melee as chakra builder |

### Out of scope for this slice

XP/levels/skill tree, open world, quests, NPC dialogue, save system, the user's photo-based character (UE Manny is the stand-in), wall-running/tree-running, awakening modes, team ultimate jutsu, final audio.

Jutsu names (Rasengan, Chidori, Amaterasu) are Naruto IP. Acceptable for a private project; display names live in `UJutsuData` so they can be renamed in one place before any publishing.

## 2. Architecture

### Project and plugins

- UE 5.8 project created from the Third Person C++ template, single game module `ShinobiCoop`.
- Engine plugins enabled: `GameplayAbilities`, `EnhancedInput`, `OnlineSubsystemSteam`, `SteamSockets`.
- Niagara is not used in this slice (mesh-based VFX, see §5).
- Git repository with Git LFS tracking `*.uasset` and `*.umap`. `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` ignored.

### Classes

| Class | Responsibility |
|---|---|
| `UShinobiAttributeSet` | Attributes: `Health`, `MaxHealth`, `Chakra`, `MaxChakra`, `Stamina`, `MaxStamina`, `SubstitutionCharges`, `MaxSubstitutionCharges`, meta attribute `IncomingDamage`. Clamping in `PreAttributeChange`; applies `IncomingDamage` → `Health` and triggers downed state in `PostGameplayEffectExecute`. |
| `AShinobiCharacterBase` | `ACharacter` + `IAbilitySystemInterface`. Owns the `UAbilitySystemComponent` (on the character, not PlayerState — nobody is destroyed on down/respawn). Grants startup abilities/effects. Handles downed state (`State.Downed` tag), death montage hold, revive. |
| `AShinobiPlayerCharacter` | Spring-arm camera, Enhanced Input binding, lock-on target, soft-target query, revive interaction. ASC replication mode `Mixed`. |
| `AShinobiEnemyCharacter` | Configured entirely by a `UEnemyData` data asset (stats, abilities, mesh, AI profile). ASC replication mode `Minimal`. |
| `UEnemyData` | `UPrimaryDataAsset`: display name, base max health, move speed, granted abilities, attack damage/cooldown, AI profile (`Melee` / `Ranged`), preferred range, is-boss flag, stun duration multiplier. |
| `AShinobiAIController` | C++ finite state machine: `Idle → Chase → Attack → Recover`. Ranged profile kites: keeps ~800 units, backs off when player closes. Uses NavMesh `MoveTo`. No Behavior Tree / StateTree assets. Runs on server only. |
| `UShinobiMovementComponent` | Custom `UCharacterMovementComponent` with a sprint flag in saved moves (no correction jitter). Double jump via `JumpMaxCount = 2`. |
| `UShinobiGameplayAbility` | Base ability: optional wind-up montage (hand-seal), cancel on `Event.Hit.Stagger` during wind-up with 50% chakra refund, cost/cooldown from `UJutsuData`, input ID binding. |
| `UJutsuData` | `UPrimaryDataAsset` per jutsu: display name, kanji glyph, input slot, chakra cost, cooldown, wind-up seconds, damage values, radii, montage, VFX/SFX references. |
| Abilities | `GA_Melee`, `GA_Fireball`, `GA_Chidori`, `GA_Rasengan`, `GA_Amaterasu`, `GA_Dodge`, `GA_Substitution`, `GA_ChakraCharge`, `GA_Revive`, plus enemy abilities `GA_EnemyMelee`, `GA_EnemyKunai` (enemy fireball reuses `GA_Fireball` with enemy data). |
| `UShinobiDamageExec` | `UGameplayEffectExecutionCalculation`: the single place damage is computed. Applies Shock bonus (+25%, consumes `State.Shocked.Bonus`), handles perfect dodge (target in i-frames with charges → damage 0 + `Event.Substitution` sent to target), sends `Event.Hit.Stagger` when final damage ≥ 20, emits damage-number cue with type (normal / shock / firestorm). |
| `AShinobiProjectile` | Server-spawned replicated projectile (Fireball, Kunai). Carries a `FGameplayEffectSpecHandle`; on overlap applies spec to target (and splash if configured). |
| `AShinobiArenaGameMode` | Wave schedule, spawning at spawn points, player-count HP scaling, break timer, full heal between waves, wave restart on wipe, victory on boss death, late-join and disconnect handling. |
| `AShinobiGameState` | Replicated: wave index, phase (`Break`, `Fighting`, `Boss`, `Victory`, `Restarting`), enemies remaining, break countdown end time. |
| `UShinobiSessionSubsystem` | `UGameInstanceSubsystem`: create/find/join/destroy session via `IOnlineSubsystem` session interface; `SHINOBICOOP` search key filter; invite-accepted handling; network/travel failure → main menu with message; Null subsystem fallback when Steam is absent. |
| UI widgets | `UShinobiHUDWidget`, `UMainMenuWidget`, `UPauseMenuWidget`, `UEnemyHealthWidget`, `UDamageNumberWidget` — widget trees constructed in C++ (no WBP layout assets). |

### Networking model

- Listen server; server authoritative for damage, projectiles, AI, hit detection, wave state.
- Player abilities use `LocalPredicted` net execution: cost, cooldown and wind-up montage predicted on the owning client, confirmed by server.
- Projectiles spawn on the server only (not predicted).
- Dodge and Chidori dash use GAS root-motion ability tasks (`ApplyRootMotionConstantForce`) so movement is predicted.
- Substitution teleport executes on the server; clients receive the corrected position.
- Attributes replicate via GAS rep-notifies. Status visuals and hit effects use Gameplay Cues so they play on every client.
- Hit-stop is local-only cosmetic (`CustomTimeDilation` on the local view), never affects simulation.

### Assets as code

Binary assets are created by Unreal Editor Python scripts (committed under `Scripts/`), re-runnable and idempotent:

- `UJutsuData` and `UEnemyData` assets with the values in §3.
- Toon post-process material and VFX materials (via `MaterialEditingLibrary`, custom HLSL nodes where needed).
- Gameplay Cue notify assets derived from C++ cue classes.
- Arena map layout (primitives, spawn points, player starts, NavMesh bounds, post-process volume).
- Animation retarget from Paragon skeletons to Manny (`IKRetargetBatchOperation`).

Scripts run headless: `UnrealEditor-Cmd.exe ShinobiCoop.uproject -run=pythonscript -script=<file>`.

Enhanced Input actions and the mapping context (KB+M and gamepad) are constructed in C++ at runtime (`UShinobiInputConfig`), so no input assets exist.

## 3. Gameplay rules and numbers

All numbers are initial values stored in data assets and tuned after playtest.

### Player attributes

| Attribute | Max | Regeneration |
|---|---|---|
| Health | 500 | None during a wave; full restore between waves; revive sets 30% |
| Chakra | 100 | +2/s passive; +5 per melee hit landed; +25/s while holding Chakra Charge (movement and jump locked; usable at full chakra with no gain — the pose and aura still play; ends only on release, dodge, or any `Event.Hit.Stagger`) |
| Stamina | 100 | +20/s after 1 s without spending. Sprint costs 10/s (sprint ends at 0 stamina), dodge costs 25 (blocked below 25) |
| Substitution charges | 3 | +1 charge every 15 s |

### Movement

- Walk speed 600, sprint speed 900.
- Double jump.
- Dodge: 600 units over 0.25 s; invulnerable (i-frames) for the first 0.2 s.
- **Substitution:** if a hit arrives during dodge i-frames and the player has ≥ 1 charge: charge consumed, damage negated, a log mesh spawns at the old position, player teleports behind the attacker, attacker is staggered for 0.5 s. With no charges the dodge simply avoids damage.

### Taijutsu (melee)

- 3-hit combo: 20 / 25 / 40 damage. Third hit applies small knockback.
- Combo window: 0.5 s after each hit.
- Each landed hit grants +5 chakra.

### Jutsu

| Slot | Jutsu | Chakra | Cooldown | Wind-up | Damage | Effect |
|---|---|---|---|---|---|---|
| 1 | Fireball | 20 | 4 s | 0.4 s | 60 direct + 40 splash (r = 250) | Applies **Burn**: 10 dmg/s for 4 s |
| 2 | Chidori | 30 | 8 s | 0.6 s | 120 | Dash 1200 units, steers to target; applies **Shock** |
| 3 | Rasengan | 35 | 10 s | 0.8 s | 150 | Melee range (200), knockback 1500; on a burning target triggers **Firestorm** |
| 4 | Amaterasu | 50 | 20 s | 0.5 s | 15/s for 10 s | Instant on the first enemy under the crosshair (or lock-on target), range 3000; no valid target → activation fails (red flash, no cost); black flame cannot be removed |

### Statuses and reactions

- **Burn** (`State.Burning.Normal`): 10 dmg/s for 4 s.
- **Black Flame** (`State.Burning.Black`, Amaterasu): 15 dmg/s for 10 s; never removed by reactions.
- **Shock** (`State.Shocked`): stun 1.5 s (boss: 0.5 s via stun multiplier) and grants `State.Shocked.Bonus`: next damage instance +25%, then consumed.
- **Firestorm**: when Rasengan hits a target with any `State.Burning.*` tag → 200 damage AoE r = 500 around the target. Removes `State.Burning.Normal`; `State.Burning.Black` stays.
- **Interrupt:** any single hit dealing ≥ 20 damage during a wind-up cancels the jutsu and refunds 50% of its chakra cost. Cooldown is not started.

### Targeting

- No lock-on: Fireball and Amaterasu aim at the crosshair (camera trace). Chidori and Rasengan snap to the nearest enemy within a 15° cone of the camera forward, within their range; otherwise fire straight ahead.
- Lock-on (Tab / R3): nearest enemy on screen; camera tracks the target; all jutsu aim at it. Lock releases on target death or when beyond 3000 units.

### Feel

- Hit-stop: 0.05 s on melee hits, 0.12 s on Rasengan and Firestorm (local only).
- Camera shake on Rasengan, Chidori impact, Firestorm.
- Damage numbers: white normal, yellow Shock-boosted, orange Firestorm.

### Enemies

Enemy max health is scaled: `BaseHealth × (1 + 0.5 × (PlayerCount − 1))`, evaluated at spawn.

| Enemy | Base HP | Attack | AI |
|---|---|---|---|
| Rogue Ninja | 200 | 2-hit combo, 25 per hit, every 1.5 s | Melee: chase and attack |
| Kunai Thrower | 120 | Kunai projectile 30 dmg, range 1500 | Ranged: keep ~800 units, retreat when approached |
| Rogue Jonin (boss) | 2000 | Melee 50; Fireball 90 + Burn every 8 s | Melee; first time below 50% HP summons 2 Rogue Ninja (once); stun multiplier 0.33 |

### Waves

1. 4 × Rogue Ninja
2. 4 × Rogue Ninja + 3 × Kunai Thrower
3. 6 × Rogue Ninja + 4 × Kunai Thrower
4. Boss: Rogue Jonin

10 s break between waves; full Health, Chakra and Stamina restore at break start. Boss death → Victory.

### Downed and revive

- Health reaches 0 → `State.Downed`: abilities blocked, movement disabled, 30 s bleed-out timer.
- Teammate within 200 units holds Interact 3 s → revived at 30% Health. Releasing or moving out of range cancels.
- Bleed-out expires → `State.Dead`; respawns at the start of the next wave.
- Every connected player downed or dead → current wave restarts: remaining enemies removed, all players restored and moved to player starts, wave respawns after a 5 s banner.

## 4. Controls, HUD, menus, level

### Input mapping

| Action | KB+M | Gamepad |
|---|---|---|
| Move / Look | WASD / Mouse | LS / RS |
| Sprint (hold) | Left Shift | L3 |
| Jump | Space | A |
| Dodge / Substitution | Left Ctrl | B |
| Melee | LMB | X |
| Jutsu 1–4 | 1–4 | Hold RB + A / B / X / Y |
| Chakra Charge (hold) | C | LB (hold) |
| Lock-on toggle | Tab | R3 |
| Interact / Revive (hold) | E | Y |
| Pause | Esc | Start |

The gamepad jutsu modifier uses a separate mapping context added while RB is held, so A/B/X/Y keep their normal meaning otherwise.

### HUD

- Bottom-left: Health (green → red below 30%), Chakra (blue), Stamina (yellow), 3 substitution log pips.
- Bottom-center: 4 jutsu slots — kanji glyph (火 雷 螺 天), key label, chakra cost, radial cooldown sweep, greyed when chakra insufficient, red flash when activation fails.
- Top-left: teammate Steam names + Health bars.
- Top-center: `WAVE n/3` (or `BOSS`), enemies remaining, break countdown.
- Center: crosshair; lock-on reticle on target.
- Enemies: overhead Health bar (visible once damaged or locked), Burn/Shock icons.
- Downed: desaturated screen and bleed-out timer; teammates see a revive icon over the downed player and a "Hold E" progress ring.
- Floating damage numbers; `VICTORY` screen; `WAVE RESTARTING` banner.

Kanji render through the engine's default composite font CJK fallback; no icon textures required.

### Menus

- Main menu: Host Game, Find Games, Quit. Find Games lists only sessions with the `SHINOBICOOP` key (Steam dev app ID 480 is shared by all developers). Accepting a Steam invite joins automatically.
- Pause menu: Resume, Invite Friend (Steam overlay invite UI), Leave.
- Status line on the main menu shows "Steam not detected – LAN mode" when falling back to Null.

### Arena: "Training Grounds"

- Forest clearing ≈ 80 m diameter; perimeter ring of trees/rocks as blocking walls.
- Wooden training posts, torii gate, rocks for cover.
- Player starts at the center; 4 enemy spawn points on the perimeter.
- NavMesh bounds covering the clearing; global post-process volume with the toon material.
- Built from engine primitives with toon materials plus optional free Fab props. Full environment art is sub-project #3.

## 5. Visuals, VFX, animation, audio

### Toon look

One post-process material: lighting quantized into 3 bands (scene color divided by base color, posterized, re-multiplied), Sobel outlines from scene depth and world normal, slight saturation boost. Bright sky from the template's sky atmosphere.

### Mesh-based VFX (no Niagara)

| Effect | Construction |
|---|---|
| Rasengan | Sphere mesh, swirling panner-noise emissive material, fresnel rim, blue point light |
| Chidori | Jagged emissive mesh flickering in the hand, crackle point light, stretched trail mesh during dash |
| Fireball | Noise-distorted fire sphere + trail mesh |
| Burn | Flame shell mesh on the target |
| Amaterasu | Black-flame shell mesh with dark-purple emissive rim |
| Firestorm | Expanding fire sphere, light flash, shockwave ring mesh |
| Substitution | Log mesh + smoke puff sphere at old position |
| Chakra charge aura | Thin blue glow hugging the body: the character mesh's overlay material `M_ChakraBodyAura` (vertex-normal inflate ~2.5 cm, additive unlit, fresnel rim, faint upward noise wisps) + a soft, shadowless, diffuse-only blue point light at the capsule centre (tints the floor and nearby surfaces); ~0.15 s fade. Chakra flows around the body: 14 camera-facing wisps rise off it, 4 arc streams spiral from the feet to the head lying along their orbit, and a swirl turns on the ground (SpriteCook `T_ChakraFlow` cards); the glow pulses. Driven by `UShinobiChakraAuraComponent` following the replicated `State.ChakraCharging` tag |

All VFX are spawned through Gameplay Cues.

### Animation

- Locomotion, jump and fall from the Third Person template (Manny).
- Melee combo (`MM_Attack_01..03`), dodge (`MM_Dash`), hit reactions and deaths come with the template's Manny animations; montages are generated from them by script. Characters use the template's `ABP_Manny_Combat` (has a `DefaultSlot` montage slot).
- Cast/hand-gesture wind-ups and jutsu strikes from Fab **Paragon: Shinbi** and **Paragon: Kallari** (free), retargeted to Manny by script (needed from M2).
- Downed: death montage held on the last frame. Revive: reviver plays a channel loop.
- Hand-seal wind-ups use the closest cast animation plus a glowing-hands effect. Real finger hand seals are deferred.
- Chakra charge pose: Mixamo "Power Up" animation (downloaded by the user to `SourceArt/Mixamo/`), retargeted to Manny by script (auto-generated IK rigs + IK retargeter + `IKRetargetBatchOperation.RunBatchRetarget`), wrapped in a looping `AM_ChakraCharge` montage that `GA_ChakraCharge` plays while held (montages replicate via the ASC).
- Fallback if Paragon is unavailable: a documented list of Mixamo animations for the user to download.

### Audio

Placeholder sounds from Starter Content (fire, impacts, whoosh). Real audio is out of scope.

## 6. Error handling

| Situation | Behavior |
|---|---|
| Steam not running | Null subsystem (LAN); menu shows notice |
| Session create/find/join fails | Return to menu with the failure reason |
| Host leaves | Clients return to menu with "Host left" |
| Client disconnects | Their character is destroyed; wipe check re-evaluated |
| Player joins mid-wave | Spawns at a player start and plays immediately; HP scaling uses new player count from next spawn |
| Missing `UJutsuData` / `UEnemyData` | `ensureMsgf` log, ability or enemy skipped, no crash |
| Activation blocked (chakra, cooldown, downed, stunned) | Slot red flash; no cost spent |

## 7. Testing

- **Automation tests** (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, run headless with `-nullrhi`):
  - Damage exec: Shock bonus applied once and consumed; perfect dodge negates damage and consumes a charge; stagger threshold at 20.
  - Firestorm trigger on normal and black burn; normal burn removed, black burn kept.
  - Interrupt refund = 50% of cost, cooldown not started.
  - Enemy HP scaling for 1–4 players.
  - Wave composition matches §3.
  - Stamina costs and regen delay.
- **Smoke test script**: launches a local listen server and one client, executes cheat console commands (`Shinobi.SpawnEnemy`, `Shinobi.Cast <slot>`, `Shinobi.Damage <amount>`), asserts on log lines for damage applied, status tags added and attributes replicated to the client.
- **Manual checklist** for the user: 2-player PIE session, then a real Steam session between two PCs.

## 8. Milestones

Each milestone is playable with 2-player PIE networking.

| # | Milestone | Done when |
|---|---|---|
| M0 | Setup | Project builds; plugins enabled; git + LFS; automation test harness runs |
| M1 | Core | Movement, sprint, double jump, dodge, melee combo, chakra charge, attribute bars, training dummy takes damage — replicated to a second client |
| M2 | Jutsu | 4 jutsu with VFX, Burn/Shock/Firestorm, interrupt + refund, substitution |
| M3 | Enemies + waves | 3 enemy types with AI, wave game mode, boss, scaling, downed/revive, victory, wave restart |
| M4 | Steam | Main/pause menus, host/find/invite/join, failure and disconnect handling |
| M5 | Look & feel | Toon post-process, arena dressing, hit-stop, camera shake, damage numbers, placeholder audio |

## 9. User prerequisites

1. Visual Studio 2022 17.14+ (MSVC 14.44.35211 or newer) or Visual Studio 2026 (MSVC 14.50.35723 or newer), per `Engine/Config/Windows/Windows_SDK.json` of UE 5.8.2. Workloads: "Game development with C++" and "Desktop development with C++"; components: Windows 11 SDK 10.0.22621, Unreal Engine installer/IDE support, .NET Framework 4.6.2 targeting pack.
2. Fab: add Paragon: Shinbi and Paragon: Kallari to the project (before M2 jutsu animation work).
3. Steam installed and logged in; a second PC and Steam account for the M4 real-session test.
