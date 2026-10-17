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
| `VFX/chakra_flow_sheet.png` | top row 4 rising blue chakra wisps; bottom row 4 swirl arcs (head on the left). The chakra charge flow (`UShinobiChakraAuraComponent`) | `T_ChakraFlow` (`ESprite::ChakraWisp` / `ChakraSwirl`) |
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

Page 5, slot 3 (page 4, slot 1 until Summon Pusho took it on 2026-10-17, so all the summons share page 4): a power-up, not an attack. `UGE_Susanoo` (20 s, tag `State.Susanoo`) halves the damage its wearer takes and raises
the damage they deal by 30% (`UShinobiDamageExec`: it reads the tag on the source and on the target, burning included, so
armour against armour is x0.65), and adds a periodic +1 chakra every 0.25 s (+4/s) on top of the base regen. The jutsu is
`UGA_Susanoo` (40 chakra, 60 s cooldown); it cannot be recast while the armour is up.

The look is `UShinobiPerfectSusanooComponent` on every character (`UShinobiSusanooComponent` is now only the guard's static ribcage,
with `M_Susanoo`'s panel lines off). While the tag is on it fades in Madara's Perfect Susanoo, as in
Ultimate Ninja Storm 4. It's a 12 m armoured tengu-faced figure in blue (`SK_Susanoo` in `M_PerfectSusanoo`) standing round the caster
and facing the way they face, with a chakra sword (`SM_SusanooSword` in `M_SusanooBlade`) in each fist, a white star flaring at
each hilt and white-cyan sparks (`ESprite::Burst` cards) drifting up through it. Only the Susanoo is seen: the wearer's own meshes are hidden in game (name tag excepted) from halfway
through the fade in until the armour ends. It holds a sword stance (`A_Susanoo_Idle`) and, while its wearer moves, walks
(`A_Susanoo_Walk`, or `A_Susanoo_Run` above 9 m/s) at a rate matched to their speed; without those clips it stands. Whenever
its owner starts a melee combo montage it slashes too, with the upper body only (the spine up), only the part around its strike (0.4 s before the hands' fastest moment to 0.45 s after, found at load), at its own speed: every swing takes the next of `A_Susanoo_Slash1..N` in turn, combo or not (the owner is hidden, so most presses miss the
combo window and would all be hit 1). A swing that comes mid-slash waits for it to finish (one waits, more are dropped). It cross-fades through `UShinobiSusanooAnimInstance` (the summons' layer proxy, no animation blueprint). The tag
and the montages replicate, so every machine sees the same. While it stands, the player's camera boom pulls back to 16 m and
looks 3 m up. `M_PerfectSusanoo` (2026-10-16) is near solid, as in Storm 4: masked and lit (real shading, correct sorting), its coverage
dithered to `Opacity` x `Fade` so it dissolves in and out (`Opacity` stays 1: at 0.9 the dither read as grain). A deep royal blue,
lighter or darker by plate with the armour texture's blue, an emissive floor (`Glow`) so the shadow side stays blue, a fresnel rim
(`Rim`), clean cyan panel lines (only the texture's green above `LineLow` 0.12; the old 0.045 lit ~10% of the surface as crackle),
a few sparkles and a slow band of light climbing it. `M_SusanooBlade` is translucent and unlit: the blade's vertex colour (red 1 on
the ridge) gives a white core fading to blue edges, bright enough to bloom. `M_Susanoo` (translucent, `Opacity` 0.35) stays as the
guard ribs' look. `SUSANOO_ONLY=look` runs of `Scripts/setup_susanoo.py` rebuild just these two materials and the sword.

How it's made (sources in `SourceArt/Susanoo/`):
1. ChatGPT front, side and back sheets (`Reference/`, not committed). Then a Tripo Studio multiview model and Tripo's Humanoid
   auto-rig with the Mixamo skeleton preset (65 bones), exported as FBX into `Models/tripo_rig/`. Mixamo's own auto-rig was
   tried and dropped: it gave only 33 bones (no fingers to close round a sword), and its distance-based weights tied the skirt
   and the shoulder plates to the arms, which stretched them into sheets.
2. The clips go into `Anims/` (`susanoo_idle`, `_walk`, `_run`, `_slash1..N`). The idle and slashes are Mixamo "Sword And
   Shield" clips (the second slash mirrored), downloaded Without Skin on a Mixamo rig of the same model. The walk and run are
   Tripo Studio presets on the game's own rig, exported as FBX (Blender) with only that animation and "stay in Place" on.
3. `Scripts/character/build_susanoo.py` (Blender) scales the rig to 180 cm and retargets each clip as world-space rotation
   deltas, since the two rigs' bones rest in different orientations. It closes the fingers into a fist and writes `Build/`.
   Each clip is exported with the mesh: an armature exported alone is written in its current pose as its rest pose.
4. `Scripts/character/make_susanoo_sword.py` (Blender) builds the slim chakra blade (chipped, rippling edges; ridge vertex colour).
5. `Scripts/setup_susanoo.py` imports it all into `/Game/Shinobi/Jutsu/Susanoo/` and rebuilds `M_Susanoo` with the texture.

The rising burst is `EShinobiBurst::SusanooRise` (blue).

`Scripts/shot_chakra.ps1 -Susanoo` films it from four sides, from the gameplay camera and through a melee slash.
Shots 25-26 of `Scripts/shot_chakra.ps1 -Jutsu` show it too. `Scripts/smoke_m1.ps1` checks that the client sees its own armour.

## 10. Summons (2026-10-03)

Page 4, slots 1-4: Summon Pusho (45 chakra, 35 s cooldown; added 2026-10-17, Susanoo moved to page 5 slot 3 for him), Summon Toad (60, 45 s), Summon Misha (50, 40 s; he replaced the snake on 2026-10-14) and Summon Pabu (45, 30 s; it replaced the pack of four dogs on 2026-10-12). After the Kuchiyose seals
(`AM_Summon_Seal`, a crouch with the right fist drawn up) and a palm slam (`AM_Summon_Release`, both built by
`Scripts/character/build_jutsu_anims.py`, timeline `Summon`) the creature appears 4-5 m in front of the caster in a puff of smoke
(`EShinobiBurst::SummonPoof`). A caster has one of each kind out at a time: calling it again replaces it.

A summon is `AShinobiSummon`, an `AShinobiEnemyCharacter` on the **players'** team, so the existing enemy AI, health, attack abilities and
death do the work: it chases the nearest hostile, enemies pick it as a target like a player, and the caster's area jutsu skip it. It is
not counted by waves or camps (only `FShinobiEncounter::Spawn` registers kills). Its numbers are the table at the top of
`ShinobiSummon.cpp`:

| Kind | Health | Attack | Stays |
|---|---|---|---|
| Toad (x1) | 600 | `UGA_ToadSlam`: 60 to everything within 4.5 m of its feet, every 2.5 s | 12 s |
| Misha (x1) | 500 | `UGA_MishaBark`: 70 to everything in a 35-degree cone out to 8 m, each staggered and thrown back, every 2.2 s; hops at 380 | 12 s |
| Pabu (x1) | 450 | `UGA_PabuBite`: 45 in front, every 0.8 s, run speed 650 | 15 s |
| Pusho (x1) | 500 | `UGA_PushoPounce`: from up to 6.5 m, crouches 0.45 s, leaps 0.6 s (a 44 cm high arc landing 30 cm short of the target), claws 65 in front; every 1.6 s, run speed 600 | 15 s |

Animation is `UShinobiCreatureAnimInstance` (no animation blueprint): it picks the clip from replicated state (velocity, health, the tag
`State.SummonAttacking` that the three attack abilities grant) and cross-fades between idle, walk, run, attack, flinch (when hurt) and
death over 0.2 s instead of cutting. Walk and run play at the speed the creature actually moves (Pabu's trot covers 144 cm/s and his gallop 480,
as `build_pabu.py` prints them; Pusho's walk 165 and gallop 605; Misha's hop covers 315 cm/s; the toad plays its one move clip at 1x at top speed), and the attack clip is stretched over
the whole attack (`UGA_EnemyAttack::GetTotalSeconds`: wind-up plus recovery, plus the leap for Pusho) so the blow lands with the damage.

Following: with nothing to fight a summon walks to a post beside its caster (`GetIdlePost`; the toad behind on one side, Misha on
the other, Pabu and Pusho ahead of them), and if it is left more than 40 m behind (a teleport, a dragon ride) it reappears there in a
puff of smoke. It only picks fights with enemies within 30 m of the caster.

Where the creatures come from (all in `docs/world/asset-credits.md`):

- **Pabu**: the user's own Pembroke corgi, a Tripo Studio model (H3.1 from `SourceArt/Character/Photos/Pabu/side.png`, retopologised to
  20k triangles, auto-rigged as "Other"; the export is `SourceArt/Tripo/Pabu`). Tripo's joints are good but its skin weights are not (two
  thirds of the vertices unweighted) and its one four-legged preset (walk) fails on that rig, so `Scripts/character/build_pabu.py` keeps
  the joints, renames them (pelvis, spine, chest, neck1-2, head, two bones an ear, four a leg, six in the tail), levels the model (Tripo's
  stands mid-step, turned 20 and tipped 10 degrees, and looks 41 degrees over his right shoulder: the head is turned to the front in the
  mesh itself, fading out down the neck, before rigging), poses it square and makes that the rest pose, weights it by bone heat and keys Idle
  (looks left and right, the corgi head tilt, ear swivels and flicks, a steady tail wag),
  Walk (a trot), Run (a gallop), Bite (the jaws close 0.4 s in, `UGA_PabuBite::WindUp`), Hit and Death with IK-driven paws. True corgi
  size (0.33 m at the withers), scaled 1.6 in code. He wears `M_Creature` (base colour, Tripo's normal map and its roughness, floored at
  0.55 so the fur is not glossy; `setup_creatures.py` uses it for any creature with `<png>_Normal.png` and `<png>_Roughness.png`) and waits
  2.2 m from his caster, closer than the toad and Misha. To rerun: `blender -b -P Scripts/character/build_pabu.py` (add `-- preview [Clip...]`
  for renders in `Saved/`), then `Scripts/setup_creatures.py`.
- **Pusho**: the user's own long-haired golden tabby cat (he/him). The two photos went to GPT Image 2.5 (in Tripo Studio) for a
  standing side view with the head edited into profile, then Tripo multi-view, Smart Mesh, auto-rig "Other" (export
  `SourceArt/Tripo/Cat/cat_rigged.zip`; the quadruped walk preset worked this time but tears the flank, because the weights are bad).
  `Scripts/character/build_pusho.py` keeps Tripo's 40 joints (shoulders re-parented from the neck to the chest, ears extended to their
  tips), turns the model straight (34 degrees off) and its face to the front (42 degrees), poses it square as the rest pose, and weights
  it by bone heat on a watertight voxel copy of the skin, copied back to the fur mesh (bone heat finds no solution on the fur itself; the
  whiskers copy their nearest skin). Clips: Idle (looks about, ear flicks, a low tail with a twitching tip), Walk (a cat's walk, tail up
  with the tip curled), Run (a gallop), Pounce (crouch and rump wiggle, the stretched leap, a right-paw swipe on landing; its length is
  `UGA_PushoPounce::GetTotalSeconds`, so change `POUNCE_*` in the script with the ability), Hit and Death. True cat size (0.36 m to the
  ear tips), scaled 2.75 in code: a panther-sized ninneko, 1 m tall. Base colour only, so `M_Character`. To rerun:
  `blender -b -P Scripts/character/build_pusho.py` (add `-- preview [Clip...]` for renders in `Saved/`), then `CREATURES_ONLY=Pusho`
  and `Scripts/setup_creatures.py`.
- **Misha**: the user's own plush puppy (photos in `SourceArt/Character/Photos/Misha`), a Tripo Studio model, auto-rigged by Tripo
  (export in `SourceArt/Tripo/Misha`). Tripo kept the photo's pose, so Misha sits; he stays seated and gets about in seated hops, a toy
  brought to life. Tripo's rig is not used (18 joints, one hind leg a single bone, no ears, three quarters of the vertices unweighted):
  `Scripts/character/build_misha.py` builds a rig to his proportions (pelvis, spine, chest, neck, head, two bones per floppy ear, three per
  front leg with IK paws, two per hind leg, four in the tail), weights it by bone heat (then hands the bib back from the front legs to
  the chest and the crown back from the ears to the head) and keys Idle (breathing, looks about, head tilt, an ear shake, the tail sweeping
  the ground), Move (one seated hop: the body sits still on the ground while the capsule moves on, then springs forward; the ears press
  in on the way up, flare on the way down and flop on landing), Bark (draws back, throws his head forward 0.5 s in,
  `UGA_MishaBark::WindUp`), Hit and Death (topples onto his side, held clear of the ground). Plush size (0.74 m to the top of his head),
  scaled 4.5 in code: a giant plush, 3.3 m tall. He wears `M_Creature` like Pabu. To rerun: `blender -b -P Scripts/character/build_misha.py`
  (add `-- preview [Clip...]` for clip renders or `-- rig` for joint and weight renders in `Saved/`), then `CREATURES_ONLY=Misha` and
  `Scripts/setup_creatures.py`. The old Mesh2Motion snake (`Scripts/character/export_creature.py`) is still in
  `/Game/Shinobi/Creatures/Snake` but nothing uses it.
- **Toad**: a Hyper3D Rodin model (`SourceArt/Creatures/Toad/rodin_toad.glb`, one unrigged mesh). Mesh2Motion has no toad, so
  `Scripts/character/build_toad.py` builds a 17-bone quadruped rig to its proportions, weights it by distance (Blender's bone-heat
  weights failed on the mesh) and keys the clips: idle, a hop for movement, a slam (rears up, crashes down at 0.7 s, which is
  `UGA_ToadSlam::WindUp`), hit and death. To rerun: `blender -b -P Scripts/character/build_toad.py` (add `-- preview`
  for pose renders in `Saved/`), then `Scripts/setup_creatures.py`.
- To add a creature: put it in `CREATURES` in `setup_creatures.py`, a look in `CreatureLookFor` and a row in `SummonStatsFor`.

Shots 27-30 (29c/29d Pusho) of `Scripts/shot_chakra.ps1 -Jutsu` show the four arrivals and the full page-4 HUD; `Scripts/smoke_m1.ps1` checks the client
sees Pabu. Not done: turn clips, a jaw (Pabu bites with a lunge and a head shake; his mouth stays shut). A summon leaves the moment its caster is downed,
dies or leaves the game, and one cast next to a wall is pulled back in front of it and put on the floor.

## 11. Dragons: Tairn and the crimson dragon (2026-10-03)

Page 5 has two dragons, each 70 chakra and a 60 s cooldown of its own: slot 1 Summon Tairn (the dragon of Fourth Wing) and slot 2 Summon Crimson Dragon (the
first dragon; ordinary orange fire). They are `UGA_SummonDragon` and `UGA_SummonCrimsonDragon` over one base (`UGA_SummonDragonBase`), differing in the
`EShinobiDragonKind` they pass to `AShinobiDragonMount::Spawn`. After the Kuchiyose seals the caster rides the dragon for `UGE_Mounted::Seconds`
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
- **Where it goes:** straight out of the dragon's mouth, in the direction the dragon faces, tipped down 10 degrees (`AShinobiDragonMount::BreathPitch`, enough to bring it
  onto things below a flying dragon). While mounted the character (so the dragon) turns to the
  camera's yaw instead of its movement (`AShinobiPlayerCharacter::Tick` swaps `bOrientRotationToMovement` for `bUseControllerDesiredRotation`), so you aim the fire
  by turning, and climb or dive with Space and Control to bring it onto a target. `AShinobiDragonMount::BreathDirection` and `BreathTarget` give the
  direction and the point where the fire ends (`BreathRange` ahead, or the first thing it meets); the damage cone, the jet and the HUD all use them.
  The HUD (`UShinobiHUDWidget::UpdateDragonCrosshair`) draws a ring with a dot at that point while you are mounted, in the dragon's fire colour.
- **Ground fire:** where the fire falls on the ground (the floor within 4 m below the jet's end; a dragon flying high leaves none) each pulse lays an
  `AShinobiFirePatch` of kind `TairnFire` (blue-white flames cut from the dragon's own sheet, dark blue-grey smoke, blue embers) or `CrimsonFire` (orange
  flames, smoke, embers): 4 s (`DragonDuration`), scale 1.3, catching enemies in it with Burn every half second, and a scorch mark on the ground afterwards,
  like a Fireball's or Amaterasu's. Pulses closer than 1.4 m to the last patch add none, so a still breath leaves one and a sweeping one leaves a trail.
- **The jet** is `AShinobiDragonFlame` (`VFX/`): one replicated actor the server turns with the dragon's facing; every machine animates its own particles
  from the clock (about 60-70 billboard sprites). Each dragon has its own SpriteCook sheet (`tairn_fire_sheet.png`: blue-white plasma flame laced with
  lightning; `crimson_fire_sheet.png`: orange dragonfire with embers; `ESprite::TairnFire` / `CrimsonFire`, 8 tall jets in a 4x2 atlas, imported by
  `Scripts/setup_sprite_vfx.py`). A tongue is born at the mouth, flies down the fire with its long axis along it (a billboard that turns about the jet's
  axis to face the camera) and widens as it goes in a narrow cone, so the overlapping tongues read as one stream; hot core tongues, a muzzle flash and, per
  dragon, lightning forks (Tairn) or embers and smoke (crimson) ride with it, under a coloured light. Tairn's fire colour is not stated anywhere in the
  books or the fan wikis, so it is blue-white and lightning-tinged (the user's choice; Violet wields lightning through him). Targets that are hit get a
  lightning burst (Tairn) or fire burst (crimson); the Burn status they catch is the ordinary orange one. `State.DragonBreathing` plays the dragon's head clip.
- **Banking:** the dragon leans into turns and pitches with climbs and dives, about the seat so the rider stays on it and upright: the actor sits at the seat
  and the body hangs off it by the saddle offset, so `AShinobiDragonMount::UpdateBank` just rotates the actor. Roll follows the rider's yaw rate (0.12 degrees of roll
  per degree a second, 35 degrees at most; right wing down in a right turn), pitch follows the vertical speed (25 degrees at full flight speed); both ease in at 4 a
  second. Cosmetic and computed on each machine from the replicated rotation and velocity. The fire and the crosshair still use the rider's facing, not the lean.
- **Camera:** the boom eases from 450 to 900 while mounted, and only then (anything else that sets the boom is left alone).
- **The dragon** is `AShinobiDragonMount`, a replicated actor with no collision or health, attached to the rider's mesh so it is smoothed on clients
  with the rider (the transform uses the mesh's default offset, not its current network-smoothed one).
- **The kind** (`EShinobiDragonKind`, replicated with the dragon and its flame) picks the model, the scale, the saddle, the mouth and the fire: a `FDragonLook`
  table at the top of `ShinobiDragonMount.cpp` (Tairn 4x, the crimson dragon 3.5x) and the particle recipes in `AShinobiDragonFlame::BeginPlay`. Dismounting
  and the breath work the same for both; the breath's hit burst is lightning for Tairn and fire for the crimson one. Adding a third dragon is a new
  kind, a look row, a recipe and a jutsu subclass.
- **Tairn's model** is a Hyper3D Rodin generation prompted from the books (a Black Morningstartail: thick ridged black scales, a mane of pointed neck
  spikes, giant golden eyes, scarred black wings, long taloned toes, a rock-hard underbelly; `SourceArt/Creatures/Tairn/rodin_tairn.glb`). Rodin
  gave him a thin curl for a tail tip, but a Morningstartail ends in a club studded with lethal spikes, so `Scripts/character/build_dragon.py` cuts the
  curl off and grafts on a Rodin-generated spiked ball (`rodin_tail_club2.glb`, from a second generation; the first came out as a fan of spikes) with 20
  long procedural cones, and tears ragged holes out of the wing membranes for the scars. It then builds a 34-bone rig to it (spine, neck, jaw, a tail whose
  last bone carries the club, arms, legs, five wing bones a side), weights it by distance (bone heat fails on it, like the toad), tips it nose-down about
  the hips in every flying clip so the back is level, and keys the clips: Idle (hover), Flap (climbing and slow flight), Glide (fast and level) and Breath
  (once per breath). Three material slots: the body and the ball (M_Character instances of `Tairn.png` and `TairnClub.png`) and the spikes (flat black).
  `blender -b -P Scripts/character/build_dragon.py` (add `-- preview` for pose renders in `Saved/dragon_*.png`; it prints the back height the saddle
  is measured from), then `Scripts/setup_creatures.py`. The assets are `SK_Tairn`, `ANIM_Tairn_*` under `/Game/Shinobi/Creatures/Tairn`. The icon is a SpriteCook painting
  of Tairn.
- **The crimson dragon's model** is a Tripo Studio model, auto-rigged there (`SourceArt/Tripo/Dragon`: `dragon_rigged.glb`/`.zip` as
  downloaded, the FBX and its textures unzipped in `fbx/`; a red dragon standing on all fours, wings raised). Tripo Studio cannot animate it: its
  presets and "Create Your Own Animation" work only on humanoid rigs. `Scripts/character/build_dragon_crimson.py` keeps Tripo's 103-bone rig and
  weights, cleans the weights (a toe of the doubled left hind leg reached into the head and wings, the right thigh owned a wing tip), adds a jaw
  bone, builds a flying pose (neck forward, tail straight, legs tucked, wings swung out flat by their membrane plane) and keys Idle, Flap, Glide
  and Breath on top of it. `-- preview` renders `Saved/dragon_*.png` and prints the saddle height and the snout for `ShinobiDragonMount.cpp`.
  Lit with `M_Creature` (base colour, normal, roughness). `SK_CrimsonDragon` under `/Game/Shinobi/Creatures/CrimsonDragon`; the skeleton
  changed, so delete that folder before re-importing a new rig. The first crimson dragon (a Rodin model) is in git history.
- **The seated pose** is the Mixamo "Sitting Idle" (`SourceArt/Mixamo/Sitting Idle.fbx`, imported by `Scripts/setup_jutsu_anims.py` as `AM_Mount_Sit`).
  `AShinobiPlayerCharacter::UpdateSeatedPose` loops it while mounted and replays it whenever no jutsu montage is playing. Without that file the script
  falls back to a pose built by `build_jutsu_anims.py` (timeline `Mount`).

Shots 31-34 of `Scripts/shot_chakra.ps1 -Jutsu` show the ride, the rider close up, the breath, a climb and the page-5 HUD; `Scripts/smoke_m1.ps1` checks
the client sees itself mounted and the dragon actor. Not done: the rider cannot steer by pitch (Space and Control are the only vertical control), and the eyes are Rodin's small golden ones.
