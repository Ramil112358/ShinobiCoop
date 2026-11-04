# Performance and clean-up audit (UE 5.8), 2026-11-03

This audit was done without running the editor. Section 1 lists the changes already made in this branch and how to check them.
Section 2 lists suggestions, ordered from quick wins to bigger changes. Each one says what to change, why it should help, and
how to measure it. None of the suggestions is applied yet: each needs a local build, a profile and a look at the result.

**Measure before and after every change.** Use `stat unit` / `stat unitgraph` (game, draw, GPU), `stat gpu`, `ProfileGPU`
(Ctrl+Shift+, in a packaged Development build) and Unreal Insights (`-trace=default,gpu,frame,cpu,loadtime -statnamedevents`).
For Nanite and Virtual Shadow Maps (VSM) use `r.Nanite.Visualize` / `r.Shadow.Virtual.Visualize` / `stat VirtualShadowMapCache`.
For game-thread cost use `stat game`, `stat anim`, `dumpticks`, and `stat net` on a listen server with two clients
(`Scripts/smoke_pvp.ps1`). The Hidden Leaf Valley (open world, villagers, foliage), London (crowd) and a PvP arena with
summons out are the three scenes worth timing.

## 1. Changes in this branch

| Change | Files | Why | How to check |
| --- | --- | --- | --- |
| Distant foliage stops swaying beyond 80 m (`AShinobiScatterActor::WindDisableDistance`, default 8000 cm; grass stops at half its 80 m cull distance). Set on new chunks and applied on load to chunks that are already built. | `Shinobi/World/ShinobiScatterActor.*` | Moving Nanite geometry (wind is World Position Offset) invalidates its cached VSM pages every frame and needs Nanite's costlier programmable raster path. Far-off sway can't be seen, but it still costs that time. | Hidden Leaf Valley: `r.Shadow.Virtual.Visualize 1` (cache mode) or `stat VirtualShadowMapCache`: fewer invalidated pages. `stat gpu`: ShadowDepths and Nanite raster times should drop. Look at trees at 60–100 m: the sway should stop without a visible pop. Tune the property on a chunk, or set it to 0 to turn the change off. |
| Overhead text (enemy HP, villager barks, player name tags) skips its per-frame camera turn while it has no text or is hidden, and stops ticking on a dedicated server. | `Shinobi/UI/ShinobiOverheadTextComponent.cpp` | Every villager, enemy, bot and summon moved its text component every frame, even with no text (villagers between barks, the local player's own tag), which costs a transform and render update each time. | `stat game` / Insights: less time in `UShinobiOverheadTextComponent` component ticks in London. Barks and HP labels still face the camera. |
| Wall-run and water-walk glow returns early while it's off (almost all the time). | `Shinobi/VFX/ShinobiSurfaceFxComponent.cpp` | It used to set a material parameter, a light intensity and two visibilities every frame, on every character. | Wall-run: the glow fades in and out as before. |
| HUD labels are only handed to Slate when their text changes (name, HP, chakra, wave, team, opponents, boosts, banner, hint, compass, cooldown digits). | `Shinobi/UI/ShinobiHUDWidget.cpp` | This avoids building an FText and updating a Slate attribute about 20 times per frame for labels that rarely change. Epic's UMG guidance is to update on change, not every tick. | HUD looks and behaves as before. `stat slate` shows slightly less. |
| Dead code removed: `UShinobiHUDWidget::GetJutsuSlotWidth`, `IsDragonCrosshairShown` (test helpers no test used) and the write-only `SlotColumns` array; `UShinobiMainMenuWidget::SetFocusTarget`; `UShinobiMenuWidget::SetEntryLabel`; `UShinobiBodyComponent::SetBodyMorph`; `UShinobiEffect::IgnoreApplicationIfTargetHas`; `ShinobiVfx::Colors::BlackFlameCore`; `ShinobiMutable::WearParameter`. | several | Nothing referenced them (C++, tests, Python, PowerShell). None was a UFUNCTION. | Compiles. `Scripts/run_tests.ps1` passes. |
| Config: duplicate `r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange` line removed; `[UnrealEd.SimpleMap]` removed from DefaultEditor.ini (it pointed at the template's `/Game/ThirdPersonCPP/...` map, which doesn't exist here). | `Config/` | Tidy-up. | Editor starts as before. |

What was checked and kept on purpose:
- The template classes (`ShinobiCoopCharacter/GameMode/PlayerController`, `Variant_Combat/*`) are kept. `Lvl_ThirdPerson` and `Lvl_Combat` are the
  "Test Level" and "Combat Level" in the session map list, and their Blueprints derive from these classes.
- The `*Shot.cpp` screenshot harnesses are kept: `Scripts/shot_chakra.ps1` uses them.
- The editor-only libraries (`ShinobiWorldBuilderLibrary`, `ShinobiMutable*Library`, ...) are already compiled out of game builds by `WITH_EDITOR`.
- Much of the code is already frame-aware: effect components only tick while shown, cloth simulation is suspended when far or unseen,
  the AnimGen crowd driver only animates when seen, follower meshes use leader pose and LODSync, and animation instances use proxies.

## 2. Suggestions (ask your local agent to implement, then profile)

### Quick wins (config or a few lines; test visually and with `stat gpu`)

1. **Decide on ray tracing.** `r.RayTracing=True` and `r.RayTracing.RayTracingProxies.ProjectEnabled=True` are on, but
   `r.Lumen.HardwareRayTracing=False` and no other feature uses hardware ray tracing. That costs ray-tracing shader permutations,
   cook time and BLAS memory for no visual return. There are two options:
   - Set `r.RayTracing=False` (needs a shader rebuild). Check MetaHuman hair and Lumen reflections look the same.
   - Or go the other way and turn on `r.Lumen.HardwareRayTracing=True`. Epic deprecated the software detail traces in 5.6
     (this project already sets `r.Lumen.TraceMeshSDFs=0`) and is moving Lumen to one hardware path at 60 Hz. This fits if your
     minimum spec is an RTX 20-series / RX 6000-series card or better.

   Measure both on your minimum-spec GPU.
2. **Lumen Lite (5.8, Beta) is the "Medium" preset.** In 5.8, `sg.GlobalIlluminationQuality=1` / `sg.ReflectionQuality=1` select Lumen Lite
   (irradiance fields with probe occlusion), which Epic says is about 2x faster than Lumen High. The existing *Graphics quality → Medium* setting
   now gets it with no other change. Compare Hidden Leaf Valley and London at Medium vs High. If it looks good, consider
   a first-launch benchmark instead of the hard-coded High (`DefaultGameUserSettings.ini`):
   `UGameUserSettings::RunHardwareBenchmark()` + `ApplyHardwareBenchmarkResults()` once, when no saved settings exist.
3. **MegaLights.** If no level or post-process volume uses MegaLights, set `r.MegaLights.Supported=False` (new in 5.8): it removes the
   MegaLights shader compilation. Alternatively, MegaLights is Production Ready in 5.8. The jutsu effects spawn many unshadowed point lights
   (fire patches, bursts, cast FX, auras), and MegaLights could let them cast shadows cheaply. Try enabling it on one fight-heavy map and
   compare `stat gpu` → Lights.
4. **Dynamic resolution.** 5.8 enables dynamic resolution on PC (DX12/Vulkan). Add a *Dynamic resolution* row to `ShinobiSettings`
   (`UGameUserSettings::SetDynamicResolutionEnabled`), budgeted from the frame cap, with TSR as the upscaler. It holds frame rate during big jutsu moments
   (Susanoo, Fire Meteor's fluid fire, hail).
5. **Shader count.** Try `r.128BitBPPSCompilation.Allow=0` (5.7+; Epic quotes ~50k fewer shaders / 15 MiB) unless a material
   renders to a 128-bit target. Check `recompileshaders listtypes` before and after.
6. **Trim plugins.** `AnimGenExample` (example content), `Terminal` and `PCG` have no runtime use in C++. PCG is only used by the
   world scripts. Check that the AnimGen villager ABP doesn't need anything from `/AnimGenExample` at runtime: `docs/anim/animgen-spike.md`
   says the ABP was copied into `/Game/Shinobi/AnimGen`. Then disable these plugins, or restrict them to the editor with `"TargetAllowList": ["Editor"]`,
   and confirm a packaged build still runs. Fewer runtime modules and less cooked content.

### Medium (C++; one feature at a time)

7. **Animation cost of enemies, summons and bots** (the biggest likely game-thread item in waves and PvP). Only villagers, the hero's hidden
   animation driver, the Owl and the Susanoo set `VisibilityBasedAnimTickOption`; nothing enables update-rate optimisation (URO). For `AShinobiEnemyCharacter`
   and `AShinobiSummon` meshes **on clients only** (`!HasAuthority()`), try `bEnableUpdateRateOptimizations = true` and
   `VisibilityBasedAnimTickOption = OnlyTickMontagesAndRefreshBonesWhenPlayingMontages`. On the host, keep them as they are: attack traces,
   sockets and root motion need current bones. For bigger crowds, enable the **Animation Budget Allocator** plugin
   (`USkeletalMeshComponentBudgeted`; 5.8 adds many `a.budget.*` cvars) for villagers and crowd bodies first. Measure `stat anim` in a late wave.
8. **Network update rates.** No actor sets `SetNetUpdateFrequency`, so every character replicates at the default 100 Hz.
   Suggested starting points:
   - Villagers (cosmetic walkers): 5–10 Hz, `SetMinNetUpdateFrequency(2)`.
   - Enemies and summons: about 30 Hz.
   - Camps and pickups: event-driven, so make them dormant (`NetDormancy = DORM_DormantAll`, then `FlushNetDormancy()` when they change).

   Measure `stat net` and server frame time with two clients. Iris is production-ready in 5.8. Consider it once the update rates are tuned.
9. **ISM instead of HISM for the Nanite scatter** (`AShinobiScatterActor`). Nanite culls instances on the GPU, so HISM's cluster tree mostly adds
   build time and memory. Change `NewObject<UHierarchicalInstancedStaticMeshComponent>` to `UInstancedStaticMeshComponent` and make the
   `Instances` array `TObjectPtr<UInstancedStaticMeshComponent>`. HISM is-a ISM, so existing chunks still load. Then rebuild one chunk and compare
   `stat SceneRendering` / memory. Cull distances work on ISM too. Also try `ShadowCacheInvalidationBehavior = Rigid` on tree components if wind shadows
   still invalidate up close.
10. **HUD per-frame world scans.** `UpdateMiniMap` and `UpdateCompass` each run `TActorIterator<AShinobiCamp>` every frame, and
    `OpponentList` runs `TActorIterator<AShinobiCharacterBase>` every frame in PvP. Cache the camps once (they don't change during a session) and rebuild the
    opponent and team strings at about 4 Hz.
11. **Keep test harnesses out of Shipping.** The screenshot and smoke-test code ships in every build. That is over 1,200 lines in
    `ShinobiCreatorSpikeShot.cpp`, `ShinobiOutfitShot.cpp` and `ShinobiPortraitShot.cpp`, plus the shot and smoke routines in `ShinobiPlayerController.cpp`,
    the `-Shinobi*Shot` dispatch in `AShinobiPlayerController::BeginPlay`, and the Exec cheats such as `ServerDamageSelf`. Wrap the declarations and
    definitions in `#if !UE_BUILD_SHIPPING`. This matters once `package.ps1` builds Shipping, which it does not today (`BuildConfiguration=PPBC_Development`).
12. **Effects scalability.** The Fire Meteor impact uses a 3D Niagara Fluids gas simulation, and Susanoo, the dragon and hail spawn many dynamic meshes and lights.
    Give those effects a cheaper path at *Effects quality* Low/Medium (`sg.EffectsQuality`): a sprite flipbook instead of the fluid simulation, and no
    extra point lights. Check with `stat gpu` while casting Fire Meteor.
13. **PSO hitches on first casts.** Every jutsu builds dynamic materials the first time it runs. In a packaged build, play each jutsu once with
    `-logPSO`, check `stat PSOPrecache` / hitch logs, and ship a bundled PSO cache if first-cast hitches show up. 5.8 also makes precaching
    fail gracefully while shaders are still compiling.

### Bigger changes (plan first; mostly Experimental features in 5.8)

14. **Nanite Foliage** (5.7/5.8, Experimental): Nanite assemblies, skinned "Dynamic Wind" instead of WPO, voxels for distant trees, and the
    5.8 Procedural Vegetation Editor. This is the long-term fix for foliage cost. `Scripts/world/make_placements.py` already notes that
    Megaplants need `r.Nanite.AllowAssemblies` + `r.Nanite.Foliage`. Revisit when it leaves Experimental.
15. **Crowds via Mass.** If London or the valley needs many more villagers, the 5.8 Mass overhaul (entities created off the game thread) and
    MetaHuman Crowd (Experimental) are the scalable route. Keep `AShinobiVillager` for the few that talk.
16. **Mover** is still Experimental in 5.8. Keep `UShinobiMovementComponent` (CharacterMovement) for now.

Sources: [UE 5.8 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes?lang=en-US),
[Tom Looman – UE 5.8 performance highlights](https://tomlooman.com/unreal-engine-5-8-performance-highlights/),
[Tom Looman – UE 5.7 performance highlights](https://tomlooman.com/unreal-engine-5-7-performance-highlights/),
[Epic forums – VSM with Nanite foliage (WPO)](https://forums.unrealengine.com/t/vsm-with-nanite-foliage-wpo/776406),
[Epic forums – ShadowDepth cost with Nanite/VSM foliage](https://forums.unrealengine.com/t/shadowdepth-tanking-fps-with-nanite-vsm-foliage-ue-5-3/2072806).

## 3. Done (2026-11-04): quick wins, game-thread and network cost, effects, builds and hitches

Not compiled or profiled where it was written. Build, run `Scripts/run_tests.ps1`, then measure as section 2 says. The new tests are
`ShinobiCoop.Render.EffectsQualityCutsHeavyEffects`, `NetworkRatesAndDormancy`, `SummonHiddenMannequinOnlyTicksMontages` and
`ScatterUsesPlainInstancingForNanite`.

| Item | What changed | Visible difference at a given setting |
| --- | --- | --- |
| 5. Shader count | `r.128BitBPPSCompilation.Allow=0` (DefaultEngine.ini) | None. No material here renders to a 128-bit target. One that did would fail to compile, naming itself. |
| 6. Plugins | `PCG` and `Terminal` editor-only (`TargetAllowList`). `AnimGenExample` is **kept**: the villager AnimBP still uses its trained controller `AGC_AnimGenNPC`. | None. If the cook reports a missing `/Script/PCG` class, a cooked asset still uses PCG: remove its `TargetAllowList`. |
| 7. Animation | Clients: an enemy's hidden driver skeleton skips its bone refresh while its worn body is off screen (montages, notifies and timing keep running). Summons: the hidden capsule mannequin only ticks montages, on every machine. The server is unchanged (attacks read bones). Villagers were already limited (rendered-only parts, AnimGen LOD 2, driver gated on the crowd body). | None while visible. An enemy coming back on screen after 0.25 s off it may show its old pose for one frame. |
| 8. Network | Enemies and summons 30 updates/s (PvP bots 60), villagers 10 (2 when idle), camps and pickups dormant between changes (`FlushNetDormancy` before each). Players keep 100. | Clients only: villagers' and enemies' positions arrive less often, smoothed by the movement component. Barely visible; a distant villager may corner slightly wider. |
| 9. Instancing | `AShinobiScatterActor`: Nanite meshes get plain instanced components, others stay hierarchical. Run `Scripts/run_python.ps1 -TimeoutSec 3600 -Script Scripts/world/convert_scatter_ism.py` to switch already-built chunks (valley, London, the four arenas) in place. | None (Nanite already culls and LODs per instance). |
| 10. HUD | Camps are looked up every 2 s (not a scan of every actor each frame); the PvP opponent list is rebuilt 4 times a second. | The opponent list's health and DEAD can lag up to 0.25 s. |
| 11. Shipping | The smoke tests and screenshot harnesses (`Run*Shot*`, the `*Shot.cpp` files, the `-Shinobi*Shot` flags) are compiled out of Shipping; the cheats (`ShinobiDamageSelf`, `ShinobiDrainChakra` and their server RPCs, `ShinobiCast*`, `ShinobiEquipPage`, `ShinobiDumpStats`) do nothing there. | None. |
| 12. Effects scalability | Below High *Effects quality* (a Graphics quality preset sets it): the dragon's fire, fire patches and Susanoo sparks use 3/4 of their cards at Medium and 1/2 at Low; the Fire Meteor's fluid fire runs at half resolution at Medium and not at Low; effect lights light nothing at Low. | Only at Medium / Low effects: thinner fire, a softer meteor flame (none at Low), no jutsu light on the surroundings at Low. High and up are unchanged. |
| 13. Hitches | Every jutsu's effect assets (materials, textures, meshes, Niagara systems) load in the background when a local player starts (`ShinobiVfx::PreloadEffects`). Bundled PSO cache: `r.ShaderPipelineCache.Enabled=1`, `NeedsShaderStableKeys=true`, and `Scripts/pso_cache.ps1` (`-Record` plays a Development build through the jutsu harnesses with `-logPSO`, `-Expand` builds `Build/Windows/PipelineCaches/PSO_ShinobiCoop_PCD3D_SM6.spc`; package again to bundle it). | None. The script hasn't been run; its file locations follow Epic's PSO cache docs, so check them on the first run. |

Still open: the Animation Budget Allocator (needs the crowd's components swapped for `USkeletalMeshComponentBudgeted`), Iris, Nanite Foliage,
Mass crowds and Mover (items 14–16).
