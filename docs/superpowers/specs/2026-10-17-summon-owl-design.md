# Summon Owl (replaces Summon Toad)

Date: 2026-10-17. Status: builder check done, awaiting user approval.

## Intent

The user owns a snowy-owl plush and wants it in the game as a summon, the way their puppy plush became Misha and their
corgi became Pabu. She (she/her, no name yet) is a small spirit companion: she hovers at the caster's shoulder and calls
lightning down from the sky onto enemies. She replaces Summon Toad, which is retired.

## Decided by the user

- **Replaces Summon Toad**: page 4, slot 2 (`EShinobiAbilityInputID::Jutsu2`), same seal/release cast as the other
  summons. The toad's gameplay code, tests and HUD icon are retired; its assets stay unused in `/Game/Shinobi/Creatures/Toad`
  (as the snake's were when Misha replaced it).
- **Attack: lightning that crashes down from the sky onto the enemy**, not a bolt fired from the owl.
- **Picks her own targets**: the nearest hostile in range; when it dies or leaves range she moves to the next.
  Line of sight required (my call after the builder check): a target counts only if a trace from her to its chest is
  clear of world geometry, so she does not strike through buildings or at players behind cover. The bolt is drawn from
  the sky regardless of what is overhead.
- **Flies**: she hovers overhead with flapping wings for her whole stay. She never walks, hops or lands.
- **Plush size, hovering near the caster's shoulder**: not a giant. She stays with the caster and does not chase
  enemies; the lightning does the reaching.
- **Look**: the Tripo Studio model of the plush (white fur, dark spots, striped wing tops, amber eyes, felt feet), as is.
- **Lightning look**: reuses the game's existing lightning style (Chidori blue, `ShinobiVfx` branching bolt sprite and
  `EShinobiBurst::Lightning`), so it matches the rest of the game.
- **Pronouns**: she/her in code comments and docs.

## Inputs on disk

`SourceArt/Tripo/Owl/` (in the main checkout `E:\ShinobiCoop`, untracked; copy into this branch's tree):

- `IMG_9900-9902.jpeg`: the user's photos. `owl_front/left/back.jpeg`: the GPT Image views used for Tripo.
- `owl_humanoid_rigged.fbx` / `.glb`: Tripo Smart Mesh + Tripo auto-rig, **Humanoid** type. 54.5k verts, one mesh,
  T-pose with wings straight out sideways. Bounds 0.89 m wide (wingtip to wingtip), 1.0 m tall, 0.47 m deep; Y-up in the GLB.
  Skeleton: 46 Mixamo-named joints (`mixamorig:Hips`, legs to `ToeBase`, `Spine/Spine1/Spine2`, `Neck`, `Head`,
  `Left/RightShoulder-Arm-ForeArm-Hand` plus finger joints inside the wings; the right hand has fewer finger joints than
  the left).
- `owl_humanoid_rigged_walk_animation.fbx`, `..._jump_animation.fbx`: Tripo humanoid presets. Not usable for a flying
  owl; ignore them.

Known Tripo gotchas from Pabu, Misha and the crimson dragon (memory: tripo-creature-pipeline): use the FBX, not the GLB (the GLB
imports with NaN bones in Blender); expect broken skin weights (most verts unweighted) and re-weight with bone heat;
the model keeps the reference image's pose.

## What gets built

### 1. Model and clips: `Scripts/character/build_owl.py` (Blender 5.2 headless)

Modelled on `build_misha.py` / `build_pabu.py`:

- Import the FBX, keep the joints that matter, drop the finger joints (they would only tear the wing), re-weight
  with bone heat. Mapping: arms/forearms/hands = wing root/mid/tip; legs = legs; spine/neck/head as named.
- Rest pose: the T-pose as is (wings out); face +X, feet's lowest point at Z = 0 in the exported model (same conventions as
  the other creatures, see `CreatureLookFor` comments in `ShinobiSummon.cpp`).
- Keyed clips (all in place, no root motion):
  - **Hover**: a looping flap. The wings beat down and up, the body bobs a little, the legs hang down. This is her
    idle and her move; she has no walk.
  - **Strike**: a short "call the lightning" beat: wings sweep up and hold, head tips up, then a hard down-beat at the
    moment the bolt lands (`WindUp`, see below).
  - **Arrive**: optional. Without it she simply appears in the poof flapping.
  - No Hit and no Death clip (she cannot be hit or killed).
- Export via `export_creature.py` and import via a new row in `Scripts/setup_creatures.py` (`CREATURES` table) to
  `/Game/Shinobi/Creatures/Owl/` (`SK_Owl`, `ANIM_Owl_Hover`, `ANIM_Owl_Strike`).
- `-- preview` renders like the other build scripts, so the clips can be checked before UE import.

### 2. Gameplay

**Stats** (builder's call within these, tune by feel): lifetime 15 s; strike every 2.0 s; 55 damage per strike;
strike range 2000 cm from the caster, and only hostiles within `AShinobiSummon::LeashRange` of the caster; chakra cost 60, cooldown 45 s (the
toad's).

**On hit**: damage only. No stun: a stun every 2 s on the same target would lock enemies down. No shock bonus (my
call after the builder check: the damage exec consumes it on the next direct hit from anyone, so her own next bolt 2 s
later would eat it). No stagger (the user, 2026-10-17): her bolts only do
damage. The damage exec today sends `Event.Hit.Stagger` for any direct hit of 20+ (cancels wind-ups, chakra charging,
revives and shoves the target), so her damage needs a no-stagger route through `UShinobiDamageExec` (builder's choice of
mechanism: a tag or SetByCaller value the exec reads), with a test that her hit does not stagger.

**Strike timing**: on a strike she plays Strike; the bolt lands `WindUp` 0.4 s later at the target's feet (where the target
is at landing time, so it does not miss a moving enemy unfairly). She turns to face the target while striking.

**Size** (my call; the user agreed to "close to plush size" and did not correct the 40-50 cm guess): 45 cm tall, so
the 1.0 m Tripo model at scale 0.45 (wingspan about 40 cm). The build script exports at true model size like
`build_misha.py`; the game sets the scale.

**Where she flies**: a post above the caster's LEFT shoulder (my call: the camera sits 60 cm to the caster's right and
60 cm up, so the left side keeps her out of the over-the-shoulder view), about 60 cm to the side, 30 cm behind and at
shoulder height (about 160 cm above the caster's feet). She follows the post smoothly (spring/interp, not snapped), faces where the caster faces
when idle, and turns towards her target when striking. She follows the caster everywhere (sprint, wall-run, water-walk,
dragon ride, teleport) and never needs the catch-up teleport of the ground summons.

**Not a combatant**: enemies never target her, attacks never hit her, she has no health bar and does not block movement
or projectiles (no collision). She leaves only when her time is up, when the caster is downed/dead/gone, or when the
caster summons her again (replaces her). All three leave in the usual `SummonPoof` (at her small scale).

**Multiplayer**: server authoritative like every summon; the server picks targets and applies damage with the
caster's ability system as the source; the bolt effect replicates via `AShinobiBurst`. PvP: she strikes the caster's
hostiles, which includes enemy-team players, like the other summons.

### 3. The sky bolt

A bright vertical bolt from roughly 15 m above the target straight down to its feet, using the existing branching-bolt
sprite (`ShinobiVfx` `Lightning`) stretched vertically and Chidori blue, a short flash of light, and
`EShinobiBurst::Lightning` at the impact point. Lasts about 0.25 s. A thunder sound if the game's SFX set has one
(`ShinobiSfx`); otherwise none for now.

### 4. Retiring the toad

Remove: `EShinobiSummonKind::Toad`, `UGA_SummonToad`, `UGA_ToadSlam`, `Cooldown.Jutsu.SummonToad`, the toad rows in
`ShinobiSummon.cpp`, the toad icon row in `Scripts/setup_hud.py`. Update every reference: `ShinobiPlayerCharacter.cpp`
(ability list), `ShinobiPlayerController.cpp` (debug cast at 5.5 s and the jutsu-shot timeline at 52-54.4 s, renamed to Owl),
`ShinobiPvpTests.cpp` (three tests summon a toad: switch them to Misha, which is a combatant like the toad was; the team
assertions are about summons in general), `ShinobiSummonTests.cpp` (70 toad mentions). Keep `build_toad.py` and the toad assets.

New: `UGA_SummonOwl` (page 4 slot 2, `DisplayName` "Summon Owl"), `Cooldown.Jutsu.SummonOwl`, icon
`T_Jutsu_SummonOwl` (SpriteCook, low quality, as Misha's; the user's SpriteCook balance may be low, so ask before
spending).

### 5. Tests

Replace the toad's tests with the owl's, in `ShinobiSummonTests.cpp`: summoning replaces the previous owl; she
strikes the nearest hostile in range and not one beyond it
nor one behind a wall; the hit applies damage and does not stagger; she cannot be
targeted or damaged by enemies; she leaves when her time is up and when the caster goes down; her post tracks the caster. Run only
the Summons and PvP test groups during the work and the full suite once at the end (user's standing rule).

## Non-goals

- A name for her (later).
- Combat AI beyond "nearest hostile in range": no target priority, no focusing the player's target.
- Riding her, or any player interaction with her.
- Landing, perching, walking or idling on the ground.
- Hit or death reactions.
- Fixing the summon "known gaps" (asset cooking by string path, etc.).

## Anti-goals (would be a failure even if it works)

- She looks like a real owl or a cartoon. She must read as the user's plush.
- She blocks the camera's over-the-shoulder view or clips through the caster's head or arms.
- The lightning reads as coming from her, not from the sky.
- Enemies stop fighting the player to go after her.
- The toad lingers anywhere a player can see it (HUD slot, icon, jutsu name).

## Left to the builder

- How she is built in code. Recommended: a lightweight replicated actor of her own (skeletal mesh, no capsule
  collision, no AI controller, a server timer for strikes), spawned and dismissed through the same
  `AShinobiSummon::Summon` / `GA_SummonBase` path or a sibling of it. Making her an `AShinobiSummon` (an AI enemy
  character with health) would mean switching off most of what it does. The builder may pick either, provided she meets
  "not a combatant" above.
- The exact numbers inside the ranges above, the post offsets, the flap speed and the bolt look's details.
- Whether the Strike clip has an anticipation hold, and whether an Arrive clip is worth it.
- The worktree is `E:\ShinobiCoop-owl` on `feature/summon-owl` (from `main` 16e5a9c4). The main checkout has someone
  else's uncommitted Tairn work on `feature/tripo-tairn`: do not touch it. Memory "git merge vs editor lock": never
  switch or merge uasset branches with UE open.
