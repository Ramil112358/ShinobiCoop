# Sprite VFX, HUD kit and how to add a jutsu that uses them

What the 2026-10-01 SpriteCook visuals pass added, how it works, how to reuse it for a new jutsu (the worked example is a
meteor shower: flames, impact bursts and scorch marks), and where the existing effects can still be improved.
Asset ids, prompts and the UI-kit id are in `SourceArt/SpriteCook/spritecook-assets.json`; licences and credits are in
`docs/world/asset-credits.md`.

## 1. What exists

### Art (generated with SpriteCook, original, no third-party licence)

| File (SourceArt/SpriteCook/) | What it is | Imported as |
|---|---|---|
| `VFX/flames_sheet.png` | 8 realistic fire tongues, 4 x 2 atlas on pure black | `/Game/Shinobi/VFX/T_Flames` |
| `VFX/smoke_embers_sheet.png` | top row 4 smoke plumes, bottom row 4 ember sprays | `T_SmokeEmbers` |
| `VFX/energy_sheet.png` | top row 4 lightning bolts; bottom row ring, vortex, slash, burst | `T_Energy` |
| `VFX/scorch_rgba.png` | charred ground mark, alpha cut from `scorch.png` (which was on white) | `T_Scorch` |
| `UI/hud_final/*.png` | HUD kit pieces cropped from the UI-kit sheet, plus four status medallions | `/Game/Shinobi/UI/Hud/T_Hud_*` |
| `World/forest_path*.png` | terrain candidates, **not used** (albedo only, smeared seams) | - |

Atlas cell index (0-7, row-major, 256 x 512 px each): `CellU = index % 4`, `CellV = index / 4`.

### Materials (built by `Scripts/setup_sprite_vfx.py`, re-runnable, close the GUI editor first)

| Material | Blend | Use | Parameters |
|---|---|---|---|
| `M_Sprite` | additive, unlit | fire, embers, lightning, rings, bursts. Black in the sheet is "nothing", so no alpha is needed | `Atlas`, `CellU`, `CellV`, `Tint`, `Intensity`, `Fade` |
| `M_SpriteDark` | translucent | Amaterasu's black flame: sheet luminance is coverage, only the dim outer edge glows in `Tint` | same |
| `M_SpriteSmoke` | translucent | smoke: luminance is coverage, `Tint` is the smoke colour | same plus `Gain` |
| `M_SpriteScorch` | translucent | ground mark that fades itself in and out from the world clock, so it can outlive its actor | `Tint`, `Start`, `Linger` |

All of them render after the toon post-process (`MTP_AFTER_DOF`), like every other jutsu effect.

### C++ API (`Source/ShinobiCoop/Shinobi/VFX/ShinobiVfx.h`)

```cpp
using ShinobiVfx::ESprite;   // Flame, BlackFlame, Smoke, Ember, Lightning, Ring, Vortex, Slash, Burst

UMaterialInstanceDynamic* MakeSprite(Outer, ESprite, Variant, Tint, Intensity);  // one quad material
void SetSpriteVariant(Material, ESprite, Variant);                               // flipbook-style flicker
int32 SpriteVariants(ESprite);                                                   // 8 flame, 4 smoke/ember/lightning, 1 for the rest
void SpawnScorch(World, Location, Radius, Tint, Linger);                         // local, cosmetic, traces to the floor
```

`Tint` white keeps the sheet's own colours (right for fire). Use it to recolour the others (blue lightning, violet black
flame). `Intensity` ~1 for fire (more blows out under bloom), 2-4 for lightning, rings and bursts.

### Where it is used

| Effect | File | How |
|---|---|---|
| Lasting fire after Fireball, Amaterasu with no target | `VFX/ShinobiFirePatch.cpp` | 10 flame cards, 3 smoke plumes (fire only), 2 ember cards, point light, scorch under it |
| Flames on burning bodies | `VFX/ShinobiBodyFlameComponent.cpp` | 10 flame cards attached to bones, one material pair per card |
| Impact bursts: Fire, Firestorm, Lightning, Spiral, BlackFlameIgnite, FireBreath | `VFX/ShinobiBurst.cpp` (`BuildCards`) | cards instead of the glow sphere |
| HUD frames, bars, slots, pips, compass, status icons | `UI/ShinobiHUDWidget.cpp` | image brushes from `T_Hud_*`; falls back to the drawn look if a texture is missing |

## 2. How a sprite effect is built

1. **Quad:** `/Engine/BasicShapes/Plane` (100 x 100 units, lies flat). Standing cards are rolled 90 degrees and turned about
   the vertical to face the camera each tick: `SetWorldRotation(FRotator(0, FaceYaw, 90))` with
   `FaceYaw = atan2(CamY - Y, CamX - X) - 90`. Size with `SetWorldScale3D(Width/100, Height/100, 1)`. A flame fills about 40% of
   its card's width, so a card of 110 x 220 is a 45 x 220 flame. Flat cards (rings, vortexes, scorch) keep the plane's own
   orientation and spin with yaw only.
2. **Material:** `MakeSprite`, one per card when cards should flicker independently.
3. **Flicker:** every ~0.1 s `SetSpriteVariant` with a random next shape (`Variant + 1 + rand(0..2)`). Eight different
   flames cycling look like animation without a flipbook.
4. **Life:** drive `Fade` (0-1) from the effect's age. Rise in quickly (`min(Age*10, 1)`), fall off with `(1 - T)^2`.
5. **Light:** a point light per effect carries the glow (the sprites are unlit); scale its intensity by the same life value.
6. **Never on a dedicated server:** create components only when `!IsNetMode(NM_DedicatedServer)`. Effects are cosmetic;
   gameplay (damage, status) stays on the server and stays out of the sprite code.
7. **Null-safe:** `MakeSprite` returns null (with an ensure) if an asset is missing. Guard `Material->Set...` calls.

## 3. Recipe: add a jutsu that needs flames and scorch (example: Meteor Shower)

Gameplay shape: the jutsu picks N impact points around the aim point, drops a meteor on each over a few seconds, and each
impact bursts, burns enemies and leaves ground fire.

**a) Jutsu ability.** Copy an existing one (`Abilities/GA_Fireball.cpp` for projectile plus ground fire,
`Abilities/GA_Amaterasu.cpp` for "effect at a spot"). Register tags, the input slot, an icon (`T_Jutsu_<Name>`, see
`Scripts/setup_hud.py`) and a chakra cost, as the others do. Put the server logic (timers that spawn each meteor) in
`ExecuteJutsu()`, guarded by `HasAuthority`.

**b) Ground fire and scorch: no new VFX code.** At each impact point call, on the server:

```cpp
AShinobiFirePatch::Spawn(GetWorld(), EShinobiFirePatch::Fire, ImpactPoint, GetShinobiCharacter(),
                         Scale /*radius = 150 * Scale*/, UGE_Burn::DamagePerTick * GetDamageScale());
```

The patch replicates, plays its flame cards, smoke, embers and light, burns enemies of the caster standing in it (every
0.5 s), and each machine spawns its own scorch under it that outlives the fire (9 s, 14 s for black flame). For many small
fires use a small `Scale` (0.5 gives a 75-unit radius); the flame cards scale with it. Cap the number of simultaneous
patches (each is ~15 components): 6-8 is fine, 20+ will show in the frame time.

**c) A custom impact look.** Add a burst kind:

1. `ShinobiBurst.h`: add `MeteorImpact` to `EShinobiBurst` **at the end** (the enum is replicated as a byte and indexes the
   `Sounds[]` array in `BeginPlay`).
2. `ShinobiBurst.cpp`: add a sound to `Sounds[]` in the same position, a line in the `switch` that sets
   `Duration`, `MaxScale`, `Intensity`, `LightIntensity`, and a `case` in `BuildCards()`:

```cpp
case EShinobiBurst::MeteorImpact:
    Duration = 0.8f;
    AddCard(Plane, ESprite::Burst, FLinearColor(1.f, 0.7f, 0.3f), 1.5f, FVector::ZeroVector, FVector2D(500.f, 500.f) * S);   // flash
    AddCard(Plane, ESprite::Ring,  FLinearColor(1.f, 0.55f, 0.2f), 1.5f, FVector::ZeroVector, FVector2D(900.f, 900.f) * S, 0.f, true); // ground ring
    for (int32 i = 0; i < 6; ++i)   // flames
        AddCard(Plane, ESprite::Flame, White, 1.2f, Around(Random.FRandRange(0.f, 120.f * S), Random.FRandRange(0.f, 2.f * PI)), FVector2D(160.f, 320.f) * S, 80.f * S, false, true);
    AddCard(Plane, ESprite::Ember, White, 2.f, FVector::ZeroVector, FVector2D(300.f, 600.f) * S, 120.f * S);
    break;
```

   `AddCard` arguments: sprite, tint, glow intensity, offset, size (width x height), rise (units climbed over the effect),
   flat (lies on the ground), flicker (swap variants), delay (seconds before it appears).
3. Spawn it with `AShinobiBurst::Spawn(World, EShinobiBurst::MeteorImpact, Point, Scale)` on the server.

**d) The falling meteor itself.** The existing `AShinobiProjectile` uses sphere shells (see section 5). For a meteor, a
`Flame` card stretched along the fall direction plus an `Ember` trail reads well; build it like a burst card but attached
to a moving actor and facing the camera.

**e) Tests.** Add one in `Tests/ShinobiJutsuTests.cpp`, patterned on `FireballLeavesGroundFire` and
`FirePatchBurnsHostilesInsideThenGoesOut`: cast, tick, assert a patch exists, enemies inside are burning, and the patch is gone
after its duration. Patch actors must stay destroyed after `Duration` (the scorch is a separate local actor, so this holds).

**f) Look at it.** `Scripts/shot_chakra.ps1 -Fire` spawns a fire patch, a black-flame patch and every burst kind at fixed
spots and screenshots them (add your burst to the `Bursts[]` table in `RunFireShotTest`). `-Jutsu` casts every jutsu at the
training dummies. Screenshots land in `Saved/Screenshots/WindowsEditor`. Take shots 0.1 s after a burst spawns; a late
shot misses a 0.4 s effect.

## 4. Generating new sprites with SpriteCook

- **Credits first** (`get_credit_balance`). On the free tier `gpt-image-2` medium 1K is 9 credits per image, a UI-kit concept
  7 and sheet 9, `gemini-3.1-flash-image` 12. A sheet of 8 sprites is one image, so always generate sheets, not single sprites.
- **VFX sheet prompt pattern:** "VFX texture sheet, 4 columns by 2 rows, N different photorealistic X, each isolated in its
  own cell, ... on a pure solid black background (#000000), no ground, no text, no borders." Use `bg_mode="include"`,
  `pixel=false`, `smart_crop=false`. Black background means additive blending needs no alpha.
- **Marks that darken the ground** (scorch, ash, blood): generate on pure white, then convert to RGBA locally
  (alpha from the darkest channel with a floor of about 0.10 so near-white haze becomes fully transparent, fade the outer 8%
  of the quad to zero, neutral colour where alpha is under 0.05). Skipping the floor gave a visible blue-tinted square.
- **Adding a sheet to the game:** put the PNG in `SourceArt/SpriteCook/VFX/`, add it to `SHEETS` in
  `Scripts/setup_sprite_vfx.py`, add an `ESprite` value and a row in the `Info()` table in `ShinobiVfx.cpp`
  (material, atlas, first cell, variant count), run the script, rebuild.
- **Record it:** add the asset id and file to `SourceArt/SpriteCook/spritecook-assets.json`.
- **Not worth it:** AI "seamless" terrain textures (hazy or smeared seams, albedo only; Poly Haven PBR is better), and
  regenerating the four painted jutsu icons.
- **New HUD pieces:** crop from the UI-kit sheet with PIL, resize near on-screen size into `UI/hud_final/`, run
  `Scripts/setup_sprite_hud.py`. A grayscale fill texture lets existing colour tints still apply.

## 5. How to improve the existing VFX and jutsu

Ordered roughly by visible gain per effort.

1. **Fireball projectile, mouth flame, hand orbs** (`ShinobiProjectile.cpp`, `ShinobiCastFxComponent.cpp`) still use
   sphere shells and procedural `M_Flame`. Replace the `Shell` and `Puff` spheres with 2-3 flame cards (add a helper that
   makes a camera-facing card on a moving actor; `ShinobiBurst::AddCard` and `ShinobiFirePatch::MakeCard` are the patterns).
2. **Fire breath** is now a row of flame cards along the burst's forward. It could follow the caster's mouth bone for the
   whole duration instead of a fixed offset, and use `Smoke` cards at the far end.
3. **Chidori arcs and Rasengan orb** (`ShinobiCastFxComponent.cpp`) use `MakeGlow` spheres and arc meshes. Use `Lightning`
   cards for the arcs on the hand and a `Vortex` card (flat or camera-facing) around the orb.
4. **Black flame on bodies** blends fire and black flame when both statuses are on, giving orange with a violet rim. Show
   only the black flame when `State.Burning.Black` is present (the tags are separate).
5. **Scorch** is a flat quad 3 units above the floor found by a downward trace. On steep slopes it can float or clip. A real
   deferred decal would conform; it needs a Substrate-compatible decal material.
6. **Smoke for Amaterasu** (dark ash plume) is missing: reuse `M_SpriteSmoke` with a purple-grey `Tint`.
7. **Sprites are unlit.** The point light supplies the glow. Add subtle light on the ground (a second, wider, dimmer light) or
   a light function if flames look pasted on at night.
8. **Real flipbooks.** `animate_game_art` can turn one flame into 8-24 frames (credits per animation). The variant-swap
   flicker is cheap and good, but a true loop would remove the random jumps.
9. **Performance.** Each patch builds ~15 components and ~15 dynamic materials; each burst up to 14. If a scene has many,
   pool the materials or share one material per kind and move the variant into per-instance custom data.
10. **HUD:** the main menu and settings screens (`ShinobiMenuWidgets.cpp`) still use the drawn look. Reuse the kit:
    panel (`ornate_square_panel`), long panels as buttons (states are in the kit sheet), track as a slider. Generate a
    menu concept with `create_ui_kit(screen_type="main-menu", style_asset_ids=[hud concept])` to match.
11. **Status icons** exist for burn, black flame, shock and stagger only. Add downed, reviving and the shock-bonus state
    to `BuildStatusIcons` and the tag list in `NativeTick`.

## 6. Pitfalls found while building this

- Close the Unreal Editor before `run_python.ps1`, `build.ps1` and the screenshot scripts; they fight over assets and DLLs.
- An actor spawned with `SpawnActor` and then given a root component starts that root at the world origin: set the
  component's world transform explicitly (`SpawnScorch` does).
- Rotator order: roll 90 first stands the plane up, then yaw turns it. Pitch/yaw alone lies flat.
- Additive sprites blow out under bloom; start at `Intensity` 1 for fire and raise only if it looks dim in daylight.
- `M_SpriteDark` needs the threshold values (rim `* 5`, coverage gain `* 5`): lower values turn the whole flame into a pink
  haze.
- Components that hold materials need null guards: `MakeSprite` is null if the asset is missing, and the automation tests
  run without every asset in some configurations.
- `ShinobiBurst` kinds are an enum replicated as a byte and an index into `Sounds[]`: append, never insert.

## 7. Jutsu pages and the movement/defense jutsu (2026-10-02)

The player has five **jutsu pages**; keys 1-4 fire the current page's jutsu (Q / wheel / RB + D-pad switch, `ShinobiPage N` on the
console). Each `UShinobiJutsuAbility` sets `Page`; the current page is a loose tag `State.JutsuPage.N`. Page 2 is Teleport,
Kamui, Almighty Push, Earth Wall; page 3 starts with Water Wall. Specs and plan: `docs/superpowers/specs|plans/2026-10-02-jutsu-pages-and-movement-defense*`.

New effects and where their look lives:

| Effect | Where | Notes |
|---|---|---|
| Push shockwave (`EShinobiBurst::Shockwave`) | `ShinobiBurst.cpp` `BuildCards` | flat `Ring` card plus five `Smoke` cards |
| Teleport arrival (`TeleportFlash`) | same | `Burst` + yellow `Ring` + three `Lightning` cards |
| Kamui swirl (`Swirl`) | same | spinning `Vortex` card + a `Burst` |
| Wall breaking (`Crumble` earth, `Splash` water) | same; spawned by `AShinobiBarrier::EndPlay` (half size when it runs out) | |
| Kamui ghost | `M_Phased` (`Scripts/setup_vfx.py`), swapped on by `UShinobiPhaseFxComponent` | additive on purpose: a translucent material scales its glow by its coverage and vanishes at 30% opacity |
| Earth Wall | `MI_PBR_rock_face` (the world library's Poly Haven rock) on the barrier's cube | the first run in a cold DDC shows the checker placeholder while its 4K textures encode |
| Water Wall | `M_WaterWall` (`Scripts/setup_vfx.py`) | translucent, flowing noise; the river's single-layer-water material was not used on a vertical slab |
| Marker kunai | `SM_MarkerKunai` (`Scripts/character/make_marker_kunai.py` -> `Scripts/setup_marker_kunai.py`) | built procedurally; two Rodin tries were rejected. The cone stands in until it is imported |

Checking them: `powershell -File Scripts/shot_chakra.ps1 -Jutsu` casts everything (pages 2 and 3 are shots 12-19) and
`Scripts/smoke_m1.ps1` checks the wall, the phase and the page over a listen-server + client pair.

## 8. Elemental jutsu: Fire Meteor, Ice Meteors, Water Dragon (2026-10-02)

Page 3 is Water Wall, Water Dragon, Fire Meteor, Ice Meteors. They share three small actors, all server-spawned and
replicated, each animated on every machine's own clock (the server alone decides damage):

| Actor | What it does | Look |
|---|---|---|
| `AShinobiTelegraph` | a ground ring that marks where a jutsu lands; an inner ring closes on the centre as the time runs out | two `Ring` sprite quads |
| `AShinobiMeteor` (`Fire`, `Ice`) | falls from the sky onto `ImpactPoint`, then damages, applies a status, and (Fire) leaves ground fire; `StartDelay` queues a shower | glowing sphere + `M_Flame` shell (no trail yet) |
| `AShinobiWaterDragon` | flies along the aim, catches each enemy once, breaks on world geometry (swept by object type) | 15 spheres in `M_WaterWall` swaying in a wave, two cone horns |

`UShinobiJutsuAbility::FindAimGround(Range, AimRadius, OutPoint)` finds where an area jutsu lands (the aim's hit, else the
ground below the end of the aim; false = fail before any cost). **Chill** (`UGE_Chill`, tag `State.Chilled`) slows movement to 60%
for 3 s (`UShinobiMovementComponent::GetMaxSpeed`) and tints the body ice-blue through the aura table.

New burst kinds (appended to the enum, with a row in the `Sounds[]` table, which is asserted): `MeteorImpact`, `FrostImpact`,
`DragonSplash`. To tune a look: the numbers are in `GA_FireMeteor.h`, `GA_IceMeteor.h`, `GA_WaterDragon.h`; the dragon's body
count, sway and colour are at the top of `ShinobiWaterDragon.cpp`.

Known limits: the dragon is a chain of spheres rather than a modelled creature; the meteors have no flame trail; there is no HUD
medallion for Chill. `Scripts/shot_chakra.ps1 -Jutsu` shots 20-24 show them; `Scripts/smoke_m1.ps1` checks the shards, the wall
and the phase replicate.

## 9. Susanoo (2026-10-03)

Page 4, slot 1: a power-up, not an attack. `UGE_Susanoo` (20 s, tag `State.Susanoo`) halves the damage its wearer takes and raises
the damage they deal by 30% (`UShinobiDamageExec`: it reads the tag on the source and on the target, burning included, so
armour against armour is x0.65), and adds a periodic +1 chakra every 0.25 s (+4/s) on top of the base regen. The jutsu is
`UGA_Susanoo` (40 chakra, 60 s cooldown); it cannot be recast while the armour is up.

The look is `UShinobiSusanooComponent` on every character: while the tag is on it fades in a 6 m ribcage guardian (`SM_Susanoo`,
standing round the caster, facing the way they face) in the additive violet `M_Susanoo`, with a slow pulse, and fades it out
when the armour ends. The model is procedural: `Scripts/character/make_susanoo.py` (Blender) builds spine, ribs, skull, arms
and legs; `Scripts/setup_susanoo.py` imports it; `M_Susanoo` comes from `Scripts/setup_vfx.py`. A modelled armour can replace
`SM_Susanoo` without code changes (origin between the feet, +X forward, about 6 m). The rising burst is
`EShinobiBurst::SusanooRise`.

Shots 25-26 of `Scripts/shot_chakra.ps1 -Jutsu` show it (the arena's walls hide the camera from one of the two angles; the other
shows the figure). `Scripts/smoke_m1.ps1` checks that the client sees its own armour.

## 10. Summons (2026-10-03)

Page 4, slots 2-4: Summon Toad (60 chakra, 45 s cooldown), Summon Snake (50, 40 s) and Summon Dogs (45, 30 s). After the Kuchiyose seals
(`AM_Summon_Seal`, a crouch with the right fist drawn up) and a palm slam (`AM_Summon_Release`, both built by
`Scripts/character/build_jutsu_anims.py`, timeline `Summon`) the creature appears 4-5 m in front of the caster in a puff of smoke
(`EShinobiBurst::SummonPoof`). A caster has one of each kind out at a time: calling it again replaces it (a dog pack of four counts as one).

A summon is `AShinobiSummon`, an `AShinobiEnemyCharacter` on the **players'** team, so the existing enemy AI, health, attack abilities and
death do the work: it chases the nearest hostile, enemies pick it as a target like a player, and the caster's area jutsu skip it. It is
not counted by waves or camps (only `FShinobiEncounter::Spawn` registers kills). Its numbers are the table at the top of
`ShinobiSummon.cpp`:

| Kind | Health | Attack | Stays |
|---|---|---|---|
| Toad (x1) | 600 | `UGA_ToadSlam`: 60 to everything within 4.5 m of its feet, every 2.5 s | 12 s |
| Snake (x1) | 350 | `UGA_SnakeStrike`: 70 from up to 8 m, every 2 s | 10 s |
| Dog (x4) | 120 each | `UGA_DogBite`: 18 in front, every 0.8 s, run speed 800 | 15 s |

Looks and animation are code-driven single-node playback (no animation blueprint): `UpdateCreatureClip` picks idle, walk, run or the
attack clip from the velocity and the replicated tag `State.SummonAttacking` (granted by the three attack abilities), so every machine
animates the same. Dogs and toad have no physics asset to ragdoll; the toad and snake play their death clip, the wolf ragdolls.

Where the creatures come from (all in `docs/world/asset-credits.md`):

- **Dogs**: the Animal Variety Pack wolf (Epic, Fab), dark coat, scaled 1.1. It faces +Y in the pack and is turned in code.
- **Snake**: the Mesh2Motion snake (CC0) with its own idle, side-winding, bite, hit and death clips, five times as thick and two and a
  half times as long as the original. `Scripts/character/export_creature.py` turns a Mesh2Motion GLB to face +X, stands it on the
  ground and writes one FBX per clip; `Scripts/setup_creatures.py` imports them under `/Game/Shinobi/Creatures/<Name>`.
- **Toad**: a Hyper3D Rodin model (`SourceArt/Creatures/Toad/rodin_toad.glb`, one unrigged mesh). Mesh2Motion has no toad, so
  `Scripts/character/build_toad.py` builds a 17-bone quadruped rig to its proportions, weights it by distance (Blender's bone-heat
  weights failed on the mesh) and keys the clips: idle, a hop for movement, a slam (rears up, crashes down at 0.7 s, which is
  `UGA_ToadSlam::WindUp`), hit and death. To rerun: `blender -b -P Scripts/character/build_toad.py` (add `-- preview`
  for pose renders in `Saved/`), then `Scripts/setup_creatures.py`.
- To add a creature: put it in `CREATURES` in `setup_creatures.py`, a look in `CreatureLookFor` and a row in `SummonStatsFor`.

Shots 27-30 of `Scripts/shot_chakra.ps1 -Jutsu` show the three arrivals and the full page-4 HUD; `Scripts/smoke_m1.ps1` checks the client
sees the four dogs. Not done: the rideable dragon (cycle E; `dragon-animations.glb` and `rig-dragon.glb` are already in
`SourceArt/Mesh2Motion`), summons walking back to their caster when nothing is in sight (they only pick enemies within 30 m of the caster and
otherwise stand still), and hit-reaction clips (the `Hit` clips are imported but unused). A summon leaves the moment its caster is downed,
dies or leaves the game, and one cast next to a wall is pulled back in front of it and put on the floor.

## 11. Tairn, the dragon (2026-10-03)

Page 5, slot 1: Summon Tairn (70 chakra, 60 s cooldown). After the Kuchiyose seals the caster rides Tairn, the dragon of Fourth Wing, for `UGE_Mounted::Seconds`
(45 s). The rider stays the player's own character: the effect grants `State.Mounted`, and `UShinobiMovementComponent::UpdateCharacterStateBeforeMovement`
(which runs in every move on the client and the server) switches to `MOVE_Flying` at 1500 u/s while the tag is on and back to falling when it goes.
No possession swap and no second ability system; the tag replicates, so the server and the predicting client agree. While mounted the movement
component also keeps the capsule floor at least `MountedGroundClearance` (200) above the ground (a downward trace in each move): the dragon's legs
hang below the rider, so a mount from the ground is a quick lift-off and a dive eases out above the floor.

- **Controls:** WASD flies along the camera's yaw; Space climbs and Control dives (A and B on a pad; `AShinobiPlayerCharacter::SetFlightClimb/Dive`).
  Melee and Dodge are blocked while mounted: the melee button breathes fire and Control is the dive. X (D-pad down on a pad) dismounts on any page
  (`UGA_DragonDismountKey`); the summon's own key does it too on page 5, like Kamui's exit. Both are free. Being downed or killed ends the ride;
  so does the timer.
- **Fire-breath** (`UGA_DragonBreath`, melee input): 5 pulses 0.3 s apart, each 25 to every hostile in a 20 degree 3D cone up to 15 m from the dragon's
  mouth, burning whoever is hit (and a fire burst on them), 15 chakra, 4 s cooldown. It is a melee-class ability for the gates: not while phased or
  charging, a jutsu cancels it, a staggering hit puts it out. The mouth is a fixed offset from the rider (`AShinobiDragonMount::MouthLocation`), the same on
  every machine, so the cone and the flames agree.
- **The jet** is `AShinobiDragonFlame` (`VFX/`): one replicated actor the server turns with the aim; every machine animates its own particles from the
  clock (about 75 billboard sprites) streaming from the mouth, widening and drifting apart, with a blue light riding it. Tairn's fire colour is not
  stated anywhere in the books or the fan wikis, so it is **blue-white and lightning-tinged** (the user's choice; Violet wields lightning through
  Tairn): the stock Flame sheet is orange and a tint only darkens it, so the jet is built from the BlackFlame sheet (its tint is its glow colour: the
  ragged flame shape), the additive Vortex sheet (the bright blue-white body and the mouth flash) and Lightning forks. Targets that are hit get a
  lightning burst; the Burn status they catch is still the ordinary orange one. `State.DragonBreathing` (granted by the ability) plays the dragon's
  head clip on every machine.
- **Camera:** the boom eases from 450 to 900 while mounted, and only then (anything else that sets the boom is left alone).
- **The dragon** is `AShinobiDragonMount`, a replicated actor with no collision or health, attached to the rider's mesh so it is smoothed on clients
  with the rider (the transform uses the mesh's default offset, not its current network-smoothed one).
- **Tairn's model** is a Hyper3D Rodin generation prompted from the books (a Black Morningstartail: thick ridged black scales, a mane of pointed neck
  spikes, giant golden eyes, scarred black wings, long taloned toes, a rock-hard underbelly; `SourceArt/Creatures/Tairn/rodin_tairn.glb`). Rodin
  gave him a thin curl for a tail tip, but a Morningstartail ends in a club studded with lethal spikes, so `Scripts/character/build_dragon.py` cuts the
  curl off and grafts on a Rodin-generated spiked ball (`rodin_tail_club2.glb`, from a second generation; the first came out as a fan of spikes) with 20
  long procedural cones, and tears ragged holes out of the wing membranes for the scars. It then builds a 37-bone rig to it (spine, neck, jaw, a tail whose
  last bone carries the club, arms, legs, five wing bones a side), weights it by distance (bone heat fails on it, like the toad), tips it nose-down about
  the hips in every flying clip so the back is level, and keys the clips: Idle (hover), Flap (climbing and slow flight), Glide (fast and level) and Breath
  (once per breath). Three material slots: the body and the ball (M_Character instances of `Tairn.png` and `TairnClub.png`) and the spikes (flat black).
  `blender -b -P Scripts/character/build_dragon.py` (add `-- preview` for pose renders in `Saved/dragon_*.png`; it prints the back height the saddle
  is measured from), then `Scripts/setup_creatures.py`. The assets keep the name Dragon (`SK_Dragon`, `ANIM_Dragon_*`); the saddle, the mouth and the
  4x scale are constants at the top of `ShinobiDragonMount.cpp`. The icon is a SpriteCook painting of Tairn.
- **The seated pose** is the Mixamo "Sitting Idle" (`SourceArt/Mixamo/Sitting Idle.fbx`, imported by `Scripts/setup_jutsu_anims.py` as `AM_Mount_Sit`).
  `AShinobiPlayerCharacter::UpdateSeatedPose` loops it while mounted and replays it whenever no jutsu montage is playing. Without that file the script
  falls back to a pose built by `build_jutsu_anims.py` (timeline `Mount`).

Shots 31-34 of `Scripts/shot_chakra.ps1 -Jutsu` show the ride, the rider close up, the breath, a climb and the page-5 HUD; `Scripts/smoke_m1.ps1` checks
the client sees itself mounted and the dragon actor. Not done: the rider cannot steer by pitch (Space and Control are the only vertical control), the
dragon does not bank with the flight, and the jet is built from stock sprite sheets (a blue dragon-fire SpriteCook sheet would sharpen it), and the eyes are Rodin's small golden ones.
