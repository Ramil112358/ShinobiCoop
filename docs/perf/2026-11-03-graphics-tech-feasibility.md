# Graphics technology feasibility: upscaling, frame generation, ray tracing, Nanite / Lumen / instancing (UE 5.8), 2026-11-03

This is research only: nothing in the project changes. It builds on `docs/perf/2026-11-03-performance-audit.md` (same date) and
does not repeat its suggestions. The editor was not run, so every number below has to be measured on real hardware first.
Vendor facts are from November 2026 and may already be out of date. Check the vendor's download page before starting any item.

## TL;DR

| Technology | Feasible now? | Hardware | Effort | Recommendation |
| --- | --- | --- | --- | --- |
| **TSR** (Epic's built-in upscaler) | Yes, already the default AA | Any SM6 GPU | Small (settings UI) | Turn *Render scale* into proper *Upscaling quality* presets and add dynamic resolution. Do this first. |
| **DLSS 4.5 Super Resolution / DLAA** | Yes. NVIDIA published a UE 5.8 plugin build | RTX 20+ | Medium | Do it. It has the biggest image-quality win for the largest user group. |
| **DLSS Frame Generation / Multi FG** (Streamline) | Yes. Same NVIDIA download, separate Streamline plugin | 2x on RTX 40+, 3x–6x and Dynamic MFG on RTX 50 | Medium | Do it, together with **Reflex**. Off by default. Label it as adding latency (matters in PvP). |
| **DLSS Ray Reconstruction** | Plugin supports it, but only useful with hardware-ray-traced Lumen | RTX 20+ | Small once HW Lumen is on | Only if you turn on hardware Lumen (see Ray tracing). |
| **DLSS 5** (neural rendering) | **No, not yet.** No official UE plugin is public. The community plugins need a runtime NVIDIA doesn't distribute | RTX 50 only (RTX 40 promised, no date) | Unknown | Wait. Revisit when NVIDIA posts an official UE 5.8 package. It also changes the art style, see below. |
| **AMD FSR 4.1 Upscaling + FSR FG 4.0 + Anti-Lag 2** | Yes. AMD's UE plugin was updated for 5.8 (Redstone SDK 2.3) | ML upscaling on RX 9000 and RX 7000. ML FG on RX 9000 (RDNA 4) only | Medium | Do it after DLSS, using the same settings plumbing. |
| **Intel XeSS 3 (SR + FG + XeLL)** | Probably. Plugin exists on Fab, **UE 5.8 support not confirmed** | SR on any vendor (DP4a). FG/MFG best on Arc, XeLL required | Medium | Lowest priority. Check the Fab listing for a 5.8 build. |
| **Hardware ray tracing (Lumen HWRT)** | Yes, the project already compiles RT shaders | RTX 20 / RX 6000 or newer | Small (config + one settings row) | Either use it (add a *Ray tracing* setting) or turn `r.RayTracing` off. Paying for it unused is the worst option. |
| **Nanite** | Already used on world meshes | SM6 | Done / ongoing | Extend carefully. Skinned characters and foliage are the open questions (see below). |
| **Lumen** | Already the GI and reflection method | SM6 | Done | Use Lumen Lite at Medium (5.8). Optional HWRT path. |
| **Instancing** | Already HISM for scatter and London props | Any | Small–Medium | ISM (not HISM) for Nanite scatter (already in the audit). Pool jutsu meshes. |

Recommended order: **(1) TSR presets + dynamic resolution → (2) DLSS SR + Reflex → (3) DLSS FG → (4) FSR → (5) ray-tracing decision →
(6) XeSS → (7) DLSS 5 when an official plugin appears.** Steps 1–3 also pay for 4 and 6, because they share the same settings code.

## 1. What the project has today

- **Engine:** UE 5.8 launcher build (`"EngineAssociation": "5.8"`). Third-party upscaler plugins therefore need prebuilt
  binaries for exactly 5.8. A source build is not required.
- **RHI:** DX12 with SM6 on Windows (`DefaultGraphicsRHI_DX12`, `PCD3D_SM6`), Vulkan SM6 on Linux, Metal SM6 on Mac. The packaging
  scripts (`Scripts/*.ps1`) are Windows, so Windows is the real target. Vendor frame generation is DX12/Windows-only in practice.
- **Rendering config** (`Config/DefaultEngine.ini`): Lumen GI + Lumen reflections (`r.DynamicGlobalIlluminationMethod=1`,
  `r.ReflectionMethod=1`), software Lumen (`r.Lumen.HardwareRayTracing=False`, `r.Lumen.TraceMeshSDFs=0`), Virtual Shadow Maps,
  Substrate, virtual textures, `r.RayTracing=True` with ray-tracing proxies. `r.AntiAliasingMethod` isn't set, so it's the UE5 default (TSR).
  `r.Nanite.MaxPixelsPerEdge=2` is set under `[SystemSettings]`.
- **Content:** Nanite is on for every world-library mesh, the Megaplant bakes and the London chunks (`Scripts/world/import_library.py`,
  `Scripts/london/import_london.py`). The open world's nature is HISM per 128 m chunk (`AShinobiScatterActor`), and London's street furniture is HISM.
  Characters are MetaHuman / Mutable skeletal meshes with groom hair (`HairStrands`) and Chaos cloth.
- **Settings UI** (`Source/ShinobiCoop/Shinobi/UI/ShinobiSettings.*`): *Graphics quality* (Low–Cinematic overall scalability),
  *Render scale* (50/67/75/100 % via `SetResolutionScaleValueEx`), *Window*, *Frame cap* (30/60/120/144/Unlimited), *VSync*,
  *Character outline*, *Snow*. Each row is a fixed option list (`RowOptions`), applied at once and saved in `GameUserSettings.ini`.
  First-launch defaults are hard-coded High (`Config/DefaultGameUserSettings.ini`).
- **Image-quality risks for any temporal upscaler or frame generation:**
  - The character outline (`M_OutlinePost`, `Scripts/world/look.py`) is a post-process material at *Scene Color After DOF*. That
    location runs **at render resolution, before the temporal upscaler**, so a thin outline drawn at 50–67 % resolution is then
    reconstructed by TSR/DLSS/FSR. Expect some shimmer on the outline at low render scales.
  - Jutsu VFX materials use the *After DOF* translucency pass (`Scripts/setup_vfx.py`, `setup_sprite_vfx.py`, `setup_ice_hail.py`,
    `setup_walls.py`), and there are many additive, emissive, fast-moving effects (fire, chakra, hail, Susanoo). Translucency writes no motion vectors
    by default. That is the classic source of **ghosting and smearing with DLSS / FSR and with frame generation**.
  - Foliage wind is World Position Offset. Its motion vectors need *Output velocity* / vertex deformation velocity, or leaves smear under
    upscaling.
  - The HUD is UMG. Frame generation has to know which pixels are UI so it doesn't interpolate them. Both vendor plugins capture UI separately,
    but it has to be checked.

## 2. Upscaling

### 2.1 TSR (built-in): do first

TSR is already running, but the menu treats it as a plain *Render scale*. Suggested changes, all in `ShinobiSettings`:

- Replace *Render scale* with **Upscaling quality**: *Native AA (100 %)*, *Quality (67 %)*, *Balanced (58 %)*, *Performance (50 %)*,
  *Ultra Performance (33 %)*. These are the same ratios the vendor upscalers use, so one row serves every upscaler.
- Add **Dynamic resolution** (On/Off): `UGameUserSettings::SetDynamicResolutionEnabled`, with the frame-cap row as the target.
  5.8 enables dynamic resolution on PC DX12/Vulkan. It holds frame rate when Fire Meteor (Niagara Fluids), Susanoo or hail fill the screen.
- Optionally add **Sharpening** (`r.Tonemapper.Sharpen`, or the vendor's own sharpening once a plugin is active).
- **First launch:** replace the hard-coded High with `RunHardwareBenchmark()` + `ApplyHardwareBenchmarkResults()` when no saved
  settings exist (also in the audit).

Effort: about a day, including tests in `Tests/ShinobiSettingsTests.cpp`.

### 2.2 NVIDIA DLSS 4.5 (Super Resolution, DLAA, Ray Reconstruction): feasible

- NVIDIA's DLSS UE plugin page lists a **DLSS 4.5 build for UE 5.8**, updated in July 2026. It includes the second-generation transformer SR model.
  Earlier 4.5 builds (8.5–8.6.1) covered UE 5.4–5.7 only, so take the 5.8-labelled package.
- Integration: copy `DLSS` (and later `Streamline`) into a new `Plugins/` folder and enable them in `ShinobiCoop.uproject`
  with `"PlatformAllowList": ["Win64"]`, so Linux, Mac and a dedicated server don't load them. Package once and check that the `nvngx_*` / `sl.*`
  DLLs land in the build.
- DLSS plugs into UE's temporal-upscaler slot (it replaces TSR while on) and follows `r.ScreenPercentage`, so the *Upscaling quality*
  row from 2.1 carries over unchanged.
- **Keep the game module free of a link dependency.** Drive everything through console variables found at runtime
  (`IConsoleManager::Get().FindConsoleVariable(...)`) and the plugins' support queries, and hide the row when the
  cvar doesn't exist. Then a build without the plugin (Linux, a teammate without it) still compiles. Prefer this to
  adding `DLSSBlueprint` / `StreamlineBlueprint` to `ShinobiCoop.Build.cs`.
- **Ray Reconstruction** only helps ray-traced (hardware) Lumen reflections and GI. With software Lumen it does nothing useful, so
  tie it to the *Ray tracing* setting (section 4).

Effort: 2–3 days, including the settings rows, a packaged-build test, and fixing VFX ghosting (section 5).

### 2.3 AMD FSR 4.1 (ML upscaling): feasible

- AMD's FSR UE plugin was **updated for 5.8 with the Redstone SDK 2.3**: FSR Upscaling 4.1.1 (ML; now also runs on RX 7000 as
  well as RX 9000), FSR Frame Generation 4.0.1 (ML, **RDNA 4 only**), and a separate **Anti-Lag 2** plugin for 5.8.
- Same plumbing as DLSS: one more option in an **Upscaler** row (*TSR / DLSS / FSR / XeSS*), shown only when the hardware supports it.
  Only one temporal upscaler can be active, so switching has to turn the others' cvars off.
- FSR expects a *reactive mask* for translucency and has its own options for it. This matters a lot for this game's additive jutsu effects.

### 2.4 Intel XeSS 3: probably feasible, lower priority

- Intel ships an XeSS 3 UE plugin (SR, FG and XeLL; Fab listing). **No source confirms a 5.8 build.** Check the Fab listing first.
- XeSS-SR runs on any vendor through DP4a, but it adds little over TSR + DLSS + FSR for this audience. XeSS-FG requires XeLL, and Intel says
  multi-frame generation is Intel-only.

## 3. Frame generation (+ latency reduction)

- **DLSS FG / MFG** comes in NVIDIA's **Streamline** UE plugin (distributed with the DLSS plugin): 2x on RTX 40+, and 3x–6x and *Dynamic*
  MFG (which picks the multiplier to hit a target) on RTX 50. **Reflex** is part of Streamline and works on almost every GeForce. Ship it on its own
  as a *Low latency* option even for players who don't use FG.
- **FSR FG 4.0.1** (ML) runs only on RX 9000. Pair it with **Anti-Lag 2**.
- **XeSS-FG** needs XeLL.

Game-specific considerations:

- **Latency and PvP.** FG interpolates frames, so input latency goes *up* compared with the same base frame rate without FG. That
  matters in the PvP arenas and the parry/block timing. Default FG **Off**, force Reflex/Anti-Lag **On** whenever FG is on,
  and say so in `RowDescription` ("Smoother motion, slightly more input delay. Needs at least ~60 FPS without it.").
- **Base frame rate.** Vendors recommend a base of about 60 FPS. Below that, artefacts and latency get worse. Combine with
  dynamic resolution rather than using FG to rescue 30 FPS.
- **Frame cap and VSync rows.** With FG on, check how `t.MaxFPS` (*Frame cap*) and the engine's VSync interact with the plugin.
  Typically you turn engine VSync off and let Reflex / the driver pace frames. The settings code should grey out or override *VSync* while FG is on.
- **UI.** Streamline separates UMG from the scene so the HUD isn't interpolated. Check the jutsu wheel, the compass, overhead
  name tags (these are world-space widgets, so they *are* interpolated) and the pause-menu blur.
- **Networking.** None. FG is purely client-side presentation. Make sure a listen-server host with FG on still simulates at its
  real rate.

Effort: 2–3 days after DLSS SR, mostly testing and the VSync / frame-cap interplay. FSR FG adds about a day once the plumbing exists.

## 4. Ray tracing

The project is in the middle: `r.RayTracing=True` builds the ray-tracing shaders and BLAS, but nothing uses them
(`r.Lumen.HardwareRayTracing=False`). Choose one:

**A. Use it, as a player setting (recommended if the minimum spec is RTX 20 / RX 6000 or newer):**
- New **Ray tracing** row: *Off* (software Lumen) / *On* (`r.Lumen.HardwareRayTracing=1`, and optionally hit lighting for reflections).
  It can switch at runtime because the RT shaders are already built.
- Benefits: more accurate reflections on London's glass and the water (`M_WaterReal`), better contact GI, and it's where Epic's
  Lumen roadmap is heading (software detail traces deprecated in 5.6). It also unlocks **DLSS Ray Reconstruction** (2.2).
- Watch out for: Nanite meshes are traced through their **fallback mesh** (London's chunks deliberately use ~1–2 % fallbacks for
  collision, so check reflections of buildings), WPO foliage in the BVH (`r.RayTracing.Geometry.*` / evaluate-WPO options),
  skinned characters and groom hair in the BVH, and VRAM. Profile Hidden Leaf Valley and London on an RTX 3060-class card.
- RT shadows are **not** recommended: Virtual Shadow Maps already give sharp, cheap shadows with Nanite.
- **Path tracing** is offline only (Movie Render Queue for trailers and screenshots). It's not a game setting.

**B. Drop it:** `r.RayTracing=False`. Fewer shader permutations, shorter cooks, less memory. This also gives up Ray Reconstruction and the HW Lumen path.

Effort: A is about 1–2 days plus profiling. B is a one-line change plus a full shader rebuild.

## 5. Making the content upscaler- and FG-friendly (needed for 2–3 to look good)

1. **Motion vectors for translucent VFX.** On the main jutsu materials (fireball, chakra aura, water dragon, hail, Susanoo),
   turn on *Output Velocity* (opaque-ish cores) or rely on the upscaler's translucency handling (TSR's translucency options, FSR's reactive mask,
   DLSS's bias / responsive options), whichever looks better. Add these flags in the `Scripts/setup_*.py` builders so they survive rebuilds.
2. **WPO foliage velocity.** Make sure the foliage material outputs velocity for its vertex deformation, or leaves smear under DLSS/FSR/FG.
3. **Outline at output resolution.** Try moving `M_OutlinePost` to *After Tonemapping* (runs after the upscaler, at output
   resolution) and compare at 50 % render scale. Check the reason it sits at *After DOF* (ordering against the toon post) is still valid
   in the arenas that no longer use `M_ToonPost`.
4. **Test matrix:** {TSR, DLSS, FSR} × {Quality, Performance} × {FG off/on} on Hidden Leaf Valley (foliage), London (crowd,
   glass) and an arena during Fire Meteor / Susanoo. Look at the outline, hair strands and particle trails.

## 6. DLSS 5: why "not yet"

- **What it is:** announced at GTC in March 2026 and shipped in a game in September 2026. DLSS 5 is *not* an upscaler or frame generator. It's a
  "3D-guided neural rendering" pass that takes the frame plus geometry, material, lighting and motion data and **adds lighting and material
  detail**. It costs frame time and is meant to run *with* DLSS SR + MFG to win that time back.
- **Hardware:** RTX 50 series only at launch. NVIDIA says RTX 40 will come later but gives no date.
- **Unreal availability:** NVIDIA says it will integrate through Streamline and a UE5 plugin, but the public DLSS download page still lists
  the **DLSS 4.5** plugin for 5.8. A developer-forum thread asking for official 5.8.2 access had no NVIDIA answer. The GitHub community plugins
  (e.g. DLSS5ForUE5, DLSS5-OneMinus) depend on the official DLSS plugin, and at least one needs a DLSS 5 runtime that isn't distributed.
  Don't ship a commercial game on those.
- **Art direction:** DLSS 5 changes how materials and lighting look. This game has a deliberate look (character outline, toon post on some maps,
  stylised VFX), and DLSS 5 could fight it. Treat it as an optional "enhanced" toggle that an artist signs off on, not as a performance feature.
- **Plan:** build sections 2–3 on Streamline now. When NVIDIA ships an official UE 5.8 (or 5.9) DLSS 5 package, it should be one more row
  (*Neural rendering: Off/On*, RTX 50 only).

## 7. Nanite, Lumen and instancing (frame-rate side)

Already in place: Nanite world meshes, Lumen GI and reflections, VSM, HISM scatter. Beyond the audit's items 2 (Lumen Lite), 9 (ISM instead
of HISM), 12 (effects scalability) and 14 (Nanite Foliage):

- **Nanite on more content:**
  - *Characters:* Nanite skinned meshes are still not Production Ready, and MetaHuman / Mutable / groom / cloth make the hero the worst
    candidate. Keep the skeletal LODs and LODSync. A fair test is the crowd bodies (CitySample crowd) in London, where many are on screen.
  - *Masked / WPO materials* (leaf cards, grass) use Nanite's programmable raster path, which costs more. Keep the 80 m wind cutoff
    (audit section 1), consider opaque geometric leaves for close trees, and move to Nanite Foliage when it leaves Experimental.
  - *Nanite tessellation / displacement* for landscape and cobbles. This is a quality feature, not a speed-up. Skip it unless an artist wants it.
  - `r.Nanite.MaxPixelsPerEdge=2` is in `[SystemSettings]` for every quality level. Consider moving it into scalability
    (`[ViewDistanceQuality@N]`-style overrides via `DefaultScalability.ini`) so Low uses 4 and Epic uses 1.
- **Lumen:** 5.8's Lumen Lite at *Medium* is the biggest free GPU win (about 2x cheaper GI by Epic's numbers). Check its look at night or in caves.
  Expose `r.Lumen.ScreenProbeGather.*` / reflection quality through scalability, not as separate rows.
  *MegaLights* (Production Ready in 5.8): the jutsu effects spawn dynamic point lights (about a dozen point-light sites in C++), and MegaLights would make
  them cheap, shadowed and plentiful. Otherwise set `r.MegaLights.Supported=False` (audit item 3).
- **Instancing:**
  - Scatter: ISM over HISM for Nanite meshes (audit item 9). Nanite already instances and culls on the GPU.
  - London street furniture is also HISM. The same change applies, but rebuild one chunk and compare first.
  - **Jutsu meshes:** effects such as hail, Susanoo ribs and kunai spawn individual mesh components. When many copies of the same mesh are alive
    (hail shards, kunai volleys), use one `UInstancedStaticMeshComponent` per effect, or Niagara mesh renderers, instead of N components.
    That cuts component and render-proxy churn and draw calls (Nanite meshes are excluded on purpose, see `setup_ice_hail.py`).
  - Non-Nanite repeated meshes already get automatic draw-call merging (dynamic instancing of mesh draw commands). Keep materials
    and material instances shared to benefit from it.
- **Other engine features worth knowing:** PSO precaching / bundled PSO cache (audit item 13), the Animation Budget Allocator and URO for
  crowds (audit item 7), async compute for Lumen (check `r.Lumen.AsyncCompute` on DX12), and the GPU *Scalability* groups.
  Variable Rate Shading does little with Nanite + Lumen. Skip it.

## 8. Proposed settings menu after all this

GRAPHICS: *Graphics quality* · *Upscaler* (TSR / DLSS / FSR / XeSS; only supported ones listed) · *Upscaling quality*
(Native AA … Ultra Performance) · *Dynamic resolution* · *Sharpening* · *Ray tracing* · *Ray Reconstruction* (DLSS + RT only) ·
*Neural rendering* (future, DLSS 5).
DISPLAY: *Window* · *Frame cap* · *VSync* (greyed out while FG is on) · *Frame generation* (Off / 2x / 3x / 4x / Dynamic; per vendor) ·
*Low latency* (Reflex / Anti-Lag 2; forced on with FG).

Implementation notes for `ShinobiSettings`:
- `RowOptions` becomes hardware-dependent. Build it from a cached capability struct, filled once at startup by probing the plugins'
  cvars and support queries, so the dropdowns don't query the RHI every frame.
- Store the vendor choices in the existing `[/Script/ShinobiCoop.ShinobiSettings]` section of `GameUserSettings.ini` (as Snow does), and re-apply
  them at startup before the first frame (e.g. from the game instance), because cvars aren't persisted by `UGameUserSettings`.
- When a saved choice is no longer supported (new GPU, plugin missing), fall back to TSR.
- Extend `Tests/ShinobiSettingsTests.cpp`: unsupported vendors aren't listed, the fallback works, and FG forces low-latency on.

## 9. Implemented: steps 1–3 and the ray-tracing setting

Code: `Source/ShinobiCoop/Shinobi/UI/ShinobiGraphics.*` (new) and `ShinobiSettings.*`. Tests: `ShinobiCoop.UI.SettingsUpscalerFrameGenRayTracing`
plus updated settings and menu tests.

Every row is always listed, like the graphics menus of current PC games (Cyberpunk 2077, Battlefield 6, The Witcher 3 next-gen). What this PC
can't use is greyed out as a whole row (as in Throne and Liberty), and still shows one of its own options: the saved pick, or Off. For
example, *Anti-aliasing* reads TSR or NVIDIA DLAA even while upscaling, never the upscaler's name. Each row's description says when it's greyed
out, and the help line under the rows says why. The reason comes from the plugin's own support query: no RTX card, driver
out of date, Hardware-accelerated GPU scheduling off, plugin not loaded... Locked options in a dropdown are greyed out and tagged *Locked*.
The rows scroll when they don't fit.

| Section | Row | Values | What it drives | Locked when |
| --- | --- | --- | --- | --- |
| UPSCALING & FRAME GENERATION | *Upscaling method* | Off / TSR / NVIDIA DLSS (first run: Off at a 100 % render scale, else DLSS where it runs) | `UDLSSLibrary::EnableDLSS` | the DLSS option: no RTX card / no plugin (`QueryDLSSSupport`) |
| | *Upscaling quality* | Quality 66.7 % · Balanced 58 % · Performance 50 % · Ultra Performance 33.3 % | screen percentage (kept while upscaling is off) | while upscaling is Off |
| | *Anti-aliasing* | TSR / NVIDIA DLAA (DLAA by default where it runs), at native resolution | `EnableDLSS` + `EnableDLAA` | while upscaling (the upscaler smooths edges itself; the pick is kept and shown greyed out); the DLAA option without DLSS |
| | *Sharpness* | Off / Low / Medium / High | `r.Tonemapper.Sharpen` 0 / 0.4 / 0.8 / 1.2 | never |
| | *Dynamic resolution* | Off / On | `UGameUserSettings::SetDynamicResolutionEnabled`; budget = frame cap (60 FPS uncapped), range 50 %–upscaling quality | never |
| | *DLSS Frame Generation* | Off / 2X … 6X / Dynamic (the plugin's own mode list; its legacy *Auto* is dropped when *OnDynamic* exists) | `UStreamlineLibraryDLSSG::SetDLSSGMode`, else `r.Streamline.DLSSG.Enable` | row: no RTX 40+ / no plugin / HAGS off (`QueryDLSSGSupport`). Options the GPU can't run (3X+ below RTX 50, from `GetSupportedDLSSGModes`) |
| | *NVIDIA Reflex Low Latency* | Off / On / On + Boost | `UStreamlineLibraryReflex::SetReflexMode` | row: no GeForce / no plugin. *Off* while frame generation is on |
| RAY TRACING | *Ray tracing* | Off / On (Off by default: today's look) | `r.Lumen.HardwareRayTracing` | no RT GPU or not DX12/Vulkan SM6 (`IsRayTracingEnabled()`) |
| | *DLSS Ray Reconstruction* | Off / On (On also turns on DLSS, or DLAA with upscaling off, and ray tracing) | `UDLSSLibrary::EnableDLSSRR` | no DLSS RR (`QueryDLSSRRSupport`) or no ray tracing |
| DISPLAY | *Screen mode* (was *Window*) | Fullscreen / Borderless / Windowed | `SetFullscreenMode` (Borderless also takes the desktop resolution) | never |
| | *Resolution* | the monitor's fullscreen modes (windowed: the sizes that fit), 1280 × 720 and up, largest first | `SetScreenResolution` + `ApplyResolutionSettings` | in Borderless (it always covers the screen) |
| | *VSync* | Off / On | engine VSync | locked off while DLSS Frame Generation is on |

`UShinobiGraphicsSubsystem` applies the saved picks at startup, after the PostEngineInit plugins have loaded. The settings panel probes again
every time it opens. The plugins' own functions do the switching. Where a function is missing, the code falls back to the console variable, set at
**code priority**. At game-setting priority, `r.Lumen.HardwareRayTracing=False` in `DefaultEngine.ini` (project-setting priority) silently won, so
the *Ray tracing* setting did nothing.

DLAA is not an *Upscaling quality*. As in Battlefield 6 (native anti-aliasing TAA / DLAA / FSR Native / XeSS Native, apart from the upscaler and its
mode) and The Witcher 3 next-gen (anti-aliasing apart from resolution scaling), it's the native-resolution *Anti-aliasing* pick, used while
*Upscaling method* is Off.

### Checking what was found: `Shinobi.GraphicsReport`

The first probe of each run writes a report to the log (`LogShinobiGraphics`, in `Saved/Logs/ShinobiCoop.log`). Typing `Shinobi.GraphicsReport` in
the in-game console (~) probes again and writes it again. It lists:
- the GPU and RHI;
- each NVIDIA plugin (found / enabled / mounted, its folder, and whether each of its modules loaded);
- the console variables above (value, or *MISSING*);
- every function of `UDLSSLibrary`, `UStreamlineLibraryDLSSG` and `UStreamlineLibraryReflex`, with parameter and return types (or *MISSING*);
- what the menu concluded (available / locked, and why) and the frame generation modes;
- `=> in effect:` what the plugins say is running now (`IsDLSSEnabled`, `IsDLAAEnabled`, `IsDLSSRREnabled`, `GetDLSSMode`, `GetDLSSGMode`,
  `GetReflexMode`) and the screen percentage. After changing a setting, run the command again to check it took.

That is how to confirm the names this code uses: each one should appear in the report. To check them in the plugin's source instead (PowerShell,
from the project folder):

```powershell
Select-String -Path Plugins\DLSS\Source\*\Public\*.h, Plugins\Streamline*\Source\*\Public\*.h -Pattern "static .*\(" | Select-Object -ExpandProperty Line
Select-String -Path Plugins\DLSS\Source\*\Private\*.cpp, Plugins\Streamline*\Source\*\Private\*.cpp -Pattern 'TEXT\("(r|t)\.(NGX|Streamline)\.' | Select-Object -ExpandProperty Line
```

### Installing NVIDIA's plugins (each Windows build machine)

1. Download the **DLSS 4.5 Unreal Engine plugin for UE 5.8** from https://developer.nvidia.com/rtx/dlss/get-started (accept the SDK licence).
2. Copy its plugin folders into `ShinobiCoop/Plugins/` (create it). `ShinobiCoop.uproject` already lists `DLSS`, `StreamlineCore`, `StreamlineDLSSG`
   and `StreamlineReflex` as **Optional**, Win64-only, not for Server targets. If the package names its folders differently (older packages had a single `Streamline`),
   change those four entries to match the `.uplugin` names.
3. Build and run. The log line `LogShinobiGraphics: Graphics: DLSS 1, Ray Reconstruction 1, Reflex 1, hardware ray tracing 1, frame generation [Off, On2X, ...]`
   shows what was found.
4. `.gitignore` now keeps `Plugins/**/Binaries/ThirdParty/` (the `nvngx_*` / `sl.*` DLLs) and still ignores built plugin binaries. Commit the plugin
   only if the repository is private and the licence allows it. Otherwise each machine installs it.

### Still to check by hand (needs a GPU)

- `stat unit` / `stat gpu` with each upscaler and quality on Hidden Leaf Valley, London and an arena during Fire Meteor / Susanoo.
- `stat DLSSG` (presented frames > 1 means frame generation is running). Check the HUD isn't smeared by frame generation.
- The image-quality items in section 5 (VFX motion vectors, foliage velocity, the outline's post-process location). These aren't done yet.
- Ray tracing on: London's glass and the water, and buildings whose Nanite fallback is coarse (the BVH uses the fallback).

## 10. Implemented: frame-rate wins (section 7) and image quality (section 5)

**Run once in the editor:** `Scripts/run_python.ps1 -Script Scripts/setup_motion_vectors.py` (with the editor closed). It changes the existing
assets in place and doesn't rebuild the VFX, the hail or the maps. The builder scripts (`setup_vfx.py`, `setup_walls.py`, `setup_ice_hail.py`,
`world/look.py`) set the same flags, so a later rebuild keeps them.

| Change | Where | Notes |
| --- | --- | --- |
| **Lighting quality** row (GRAPHICS): Low / **Medium (Lumen Lite)** / High / Epic / Cinematic | `ShinobiSettings` (`SetGlobalIlluminationQuality` + `SetReflectionQuality`) | In 5.8, Medium GI and reflections quality *is* Lumen Lite (Epic: about 2x cheaper than High). The row lets players take it alone instead of dropping the whole preset. A forum report notes dark blocks under moving objects at Medium in 5.8: check the valley at Medium. |
| **MegaLights** row (RAY TRACING), Off by default | `ShinobiGraphics` (`r.MegaLights.EnableForProject` at runtime), `ShinobiVfx::MarkEffectLight` / `ApplyEffectLight` | Needs hardware ray tracing, so it's locked without it. While it's on, every jutsu and effect light casts shadows (fireball, meteor, bursts, fire patches, dragon fire, cast flashes, auras, black flames, wall-run glow, camp fires). They're tagged `ShinobiEffectLight`, and the ones already burning switch too. Without MegaLights they stay shadowless, because each shadowed point light would cost six virtual-shadow-map views. Compare `stat gpu` (Lights) with a few jutsu on screen, Off vs On. |
| **Ice Meteor shower drawn instanced** | `UShinobiInstancedVfxSubsystem` (new), `AShinobiMeteor` | A shower used to make 60 shards × 4 mesh components (shard, frost trail, 2 chips) = 240 components. Now the shards draw from at most 4 instanced meshes (3 shard variants, the trail). Each shard is still an actor (landing, damage, replication and the bots' dodging are unchanged). Each frame's instance transforms are committed in one batch with last frame's as the previous ones, so the falling shards write motion vectors. M_IceShard / M_IceTrail need the Instanced Static Meshes usage (the script adds it). |
| Kunai: **not** instanced | — | Kunai are physical projectiles (collision, hits, replicated movement), and only a few fly at once. Instancing them would need a projectile manager. The same goes for the per-particle flame cards, which each have their own material instance (sprite frame, fade). |
| **Characters stay off Nanite** | test `ShinobiCoop.Render.CharactersStayOffNanite` | Fails if any roster body, any outfit fitted to it, or any skinned mesh a spawned player wears has Nanite turned on. Nanite skinning isn't production-ready, and MetaHuman grooms, Mutable bodies and Chaos cloth make the hero the worst case. |
| **Jutsu mesh effects write motion vectors** | `setup_motion_vectors.py`: *Output Depth and Velocity* on M_ChakraBodyAura (every glow / orb / burst / aura), M_Flame, M_FlameDark, M_Phased, M_Susanoo, M_SusanooBlade, M_WaterWall, M_PushDome, M_ChakraPillar, M_IceTrail, M_WaterCurtain | Fixes the ghosting trail behind moving jutsu with TSR, DLSS and frame generation. The soft sprite cards (M_Sprite*) are left out on purpose: their velocity would overwrite the background's behind their faint edges and smear it instead. |
| **Foliage wind writes motion vectors** | `Config/DefaultEngine.ini`: `r.Velocity.EnableVertexDeformation=1` | World Position Offset (wind, inflating jutsu shells) outputs velocity, so swaying leaves don't smear under upscaling. AMD's FSR guide lists this setting as required. It costs a little in the velocity pass. |
| **Outline after the upscaler** | `Scripts/world/look.py` (rebuilt in place by the script) | M_OutlinePost moves from *Scene Color After DOF* (render resolution, then upscaled: the 1–2 px line shimmered) to *After Tonemapping* (output resolution, sharp). Its stencil and depth lookups go through `ViewportUVToSceneTextureUV`, because the colour is at output resolution and the scene textures at render resolution. That mismatch is why outlines slid off the characters at 4K when this location was first tried. Thickness is 1.5 render pixels per 1080 render lines, never under 1. Colours are display-space now (friends darkened, enemies deep red). |

Check after running the script: an arena at *Performance* upscaling with DLSS and with TSR. Look for outlines sitting on the characters at 4K,
no ghost trail behind a Fireball or a Rasengan, a frame-generation capture of the Ice Meteor shower, and the valley's trees in wind.
`Shinobi.GraphicsReport` now also lists `r.MegaLights.EnableForProject`, `r.Lumen.FinalGatherMethod`, `sg.GlobalIlluminationQuality`,
`sg.ReflectionQuality` and `r.Velocity.EnableVertexDeformation`.

## Sources

- [UE 5.8 release notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes),
  [Unreal Engine 5.8 released (forum)](https://forums.unrealengine.com/t/unreal-engine-5-8-released/2729274),
  [GameFromScratch – UE 5.8](https://gamefromscratch.com/unreal-engine-5-8-released/),
  [Wccftech – Lumen Lite](https://wccftech.com/unreal-engine-5-8-lumen-lite-60-fps-switch-2/)
- NVIDIA: [DLSS developer page](https://developer.nvidia.com/rtx/dlss),
  [DLSS getting started](https://developer.nvidia.com/dlss-getting-started),
  [DLSS 4.5 UE 5.8 plugin update (DarknessFX)](https://x.com/DrkFX/status/2072992551045308848),
  [Plugin archive](https://forums.developer.nvidia.com/t/nvidia-dlss-unreal-engine-plugin-archive/377564),
  [Frame Generation (Streamline) UE quick start](https://d29g4g2dyqv443.cloudfront.net/sites/default/files/DLSS_Frame_Generation_Quick_Start_Guide.pdf),
  [DLSS 5 announcement](https://www.nvidia.com/en-us/geforce/news/dlss-5-3d-guided-neural-rendering/),
  [DLSS 5 faster model / rollout](https://www.nvidia.com/en-us/geforce/news/dlss-5-faster-ai-model-hogwarts-legacy-rtx-spark-laptops/),
  [Forum: official DLSS 5 plugin for UE 5.8.2](https://forums.developer.nvidia.com/t/official-dlss-5-neural-rendering-plugin-access-for-unreal-engine-5-8-2/382731),
  [Wccftech DLSS 5 roundup](https://wccftech.com/roundup/nvidia-dlss-5/),
  community plugins [DLSS5ForUE5](https://github.com/praveenkumarappu/DLSS5ForUE5) and [DLSS5-OneMinus](https://github.com/tkawecki/DLSS5-OneMinus) (unofficial)
- AMD: [FSR plugin updated for UE 5.8](https://gpuopen.com/learn/amd-fsr-plugin-updated-for-unreal-engine-58/),
  [FSR UE plugin guide](https://gpuopen.com/learn/ue-fsr/), [FSR Frame Generation](https://gpuopen.com/amd-fsr-framegeneration/),
  [FSR Upscaling](https://gpuopen.com/amd-fsr-upscaling/),
  [VideoCardz – RX 7000 + UE 5.8](https://videocardz.com/newz/amd-updates-fsr-unreal-engine-plugin-with-rx-7000-support-and-ue-5-8-compatibility)
- Intel: [XeSS developer page](https://www.intel.com/content/www/us/en/developer/topic-technology/gamedev/xess.html),
  [XeSS 3 plugin on Fab](https://www.fab.com/listings/8ab057da-982a-4e3b-8c56-ab56720d4641)
