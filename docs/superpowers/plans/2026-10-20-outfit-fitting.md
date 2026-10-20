# Outfit Fitting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The Hokage and Akatsuki cloaks fit all five roster bodies (Hero, Bika, Cat, Misha, Pabu): neat at idle with nothing of the body showing, hem and sleeves swinging and trailing when moving, tails out through an opening in the back.

**Architecture:** The master cloak GLB is fitted by Blender script once per body: sized to that body, skinned from it, with cloth regions painted into vertex colours and master-space positions stored in extra UV channels. A tail opening is cut for tailed bodies, and a hide mask of the body is baked. `setup_hero.py` imports the ten copies. An editor-only C++ library builds Chaos clothing on each copy (legacy skeletal-mesh clothing, so the user can paint and tune it in the Skeletal Mesh editor) and copies the user's tuning from one copy to the others. In game, `ApplyOutfit` wears the copy for the current body and feeds its hide mask to the body's `M_Character` opacity.

**Tech Stack:** Blender 5.2 headless Python (`bpy`, `bmesh`, `numpy`), UE 5.8 Python editor scripting, UE C++ (Chaos Cloth / `ClothingSystemEditorInterface`), UE automation tests.

**Spec:** `docs/superpowers/specs/2026-10-20-outfit-fitting-design.md` (research: `docs/character/outfit-fitting-research.md`).

## Global Constraints

- Roster bodies: Hero, Bika, Cat, Misha, Pabu, from `SourceArt/Character/Models/<Name>/<Name>_rigged.fbx` (65-bone Mixamo skeleton, bone names `mixamorig:<Bone>`).
- Outfits: Hokage, Akatsuki. Masters: `SourceArt/Character/Outfits/Hokage/Hokage_tripo.glb`, `SourceArt/Character/Outfits/Akatsuki/Akatsuki_clouds.glb`. Both are local, not committed; they are rebuilt by `prep_outfit.py` (+ `fix_cloak_clouds.py`) from the Tripo zips if missing.
- Blender: `"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup -P <script> -- <args>`. Scripts print `[<script>] ...` lines and exit 1 with `ERROR:` on failure.
- Unreal: imports run with the editor closed, via `Scripts/run_python.ps1 -Script Scripts/setup_hero.py` (`$env:SETUP_HERO_ONLY = "outfits"` for outfits only). Never switch or merge branches with uasset changes while the editor is open. Build: `Scripts/build.ps1`. Tests: `Scripts/run_tests.ps1 -Filter <prefix>`.
- Test only what changed during the work (the user's standing rule); the full suite runs once, in the last task.
- Committed outfit outputs (Git LFS, `.gitignore` negations): `<Outfit>_<Body>_outfit.fbx`, `<Outfit>_<Body>_hide.png`, `<Outfit>_outfit.png` (the texture, shared by all five copies). Intermediates stay local.
- Look targets (from the spec): idle neat with no body visible where the cloak covers it; hem and sleeves swing and trail when walking, running and jumping; no stretched strands, no holes or tears, no see-through gaps, no lingering jiggle after stopping; it must not work only on the Hero.
- Cloth is visual only and simulated on each machine; picks still replicate through `CharacterIndex` and `OutfitIndex`.
- Commit messages end with: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Switching character while an outfit is worn.** The cloak must swap to the new body's copy and hide mask, with no cloth explosion or the old body's mask left on. Test: `ShinobiCoop.Outfit.SwapBodyKeepsFit` (Task 7).
2. **Respawn, teleport, Substitution blink or a sudden move.** Cloth mustn't stretch across the jump. Test: `ShinobiCoop.Outfit.TeleportResetsCloth` (Task 7).
3. **Wall-run and water-walk (custom gravity).** The hem must hang along the character's gravity, not world down. Test: `ShinobiCoop.Outfit.ClothFollowsCharacterGravity` (Task 7).
4. **A copy missing for one body** (an outfit not rebuilt after adding a roster character). Wear nothing for that body rather than another body's copy, and log once. Test: `ShinobiCoop.Outfit.MissingCopyWearsNothing` (Task 7).
5. **Taking the outfit off** (`ShinobiOutfit None`). The body's hidden parts must come back. Test: `ShinobiCoop.Outfit.NoneRestoresBody` (Task 7).

---

## File Structure

- `Scripts/character/body_landmarks.py` (create): shared Blender helpers. Bone heads, body sections (chest, hip, belly width and depth), tail detection and the test poses for a rigged body. Imported by the fitter, the mask baker and the check.
- `Scripts/character/skin_outfit.py` (modify): becomes the per-body fitter. Per-body sizing, cloth-region vertex colours, master-space UVs, sim-friendly weights, tail opening. Output `<Outfit>_<Body>_outfit.fbx`.
- `Scripts/character/bake_hide_mask.py` (create): `<Outfit>_<Body>_hide.png` in the body's UV space.
- `Scripts/character/check_outfit.py` (create): posed acceptance check (metrics with thresholds, plus renders) for one (outfit, body) pair.
- `Scripts/character/build_outfits.ps1` (create): runs fitter, mask and check for every outfit × body.
- `Scripts/character/test_outfit_pipeline.py` (create): Blender-side asserts for the helpers (run headless).
- `Source/ShinobiCoop/Shinobi/Hero/ShinobiOutfitBuilderLibrary.h/.cpp` (create): editor-only C++ called from Python. Build the clothing on a copy, copy tuning between copies, add the tail capsule.
- `Source/ShinobiCoop/ShinobiCoop.Build.cs` (modify): cloth module dependencies.
- `Scripts/setup_hero.py` (modify): `HideMask` on `M_Character`, per-body outfit import, cloth build, `SETUP_HERO_ONLY=cloth` for tuning propagation.
- `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h/.cpp` (modify): per-body copy, hide mask, cloth reset, gravity, distance suspend.
- `Source/ShinobiCoop/Tests/ShinobiOutfitTests.cpp` (create): outfit automation tests. `ShinobiHeroTests.cpp` (modify) for the existing outfit test.
- `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h/.cpp` (modify): `-ShinobiOutfitShot` screenshot director. `Scripts/shot_chakra.ps1` (modify): `-Outfit` switch.
- `.gitignore`, `docs/character/outfit-steps.md` (modify).

---

### Task 1: Baseline branch and commit of the current work

**Files:**
- Commit as is: `Scripts/character/skin_outfit.py`, `Scripts/character/prep_outfit.py`, `Scripts/character/fix_cloak_clouds.py`, `docs/character/outfit-steps.md`, `docs/character/outfit-fitting-research.md`, `docs/superpowers/specs/2026-10-20-outfit-fitting-design.md`, this plan, `SourceArt/Character/Outfits/{Hokage,Akatsuki}/*_outfit.{fbx,png}`, and the `Content/Shinobi/Outfits/**` uassets they produced.

- [ ] **Step 1: Check the editor is closed** (`Get-Process UnrealEditor*` returns nothing). Create the branch: `git switch -c feature/outfit-fitting`.
- [ ] **Step 2: Commit only the files above.** Leave the user's other untracked folders alone (`Content/AccuCities/`, `SourceArt/Mixamo/*`, `Scripts/tripo_mcp.py` and so on).

```bash
git add Scripts/character/skin_outfit.py Scripts/character/prep_outfit.py Scripts/character/fix_cloak_clouds.py docs/character docs/superpowers SourceArt/Character/Outfits Content/Shinobi/Outfits
git commit -m "feat: Tripo cloaks (Hokage, Akatsuki) with prep and cloud-repaint scripts, outfit fitting design and plan"
```

Expected: `git status --short Scripts/character docs SourceArt/Character/Outfits` prints nothing.

---

### Task 2: Gate. Cloth on a leader-pose follower, built by script, in UE 5.8

This decides the cloth path for every later task. Build the clothing in C++ on the existing single Hero copy (`SK_Hokage`) and prove it simulates while following the body through leader pose.

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Hero/ShinobiOutfitBuilderLibrary.h`, `.cpp`
- Modify: `Source/ShinobiCoop/ShinobiCoop.Build.cs` (editor-only private deps `ClothingSystemEditorInterface`, `ClothingSystemRuntimeCommon`, `ClothingSystemRuntimeInterface`, `ChaosCloth`; add `ClothingSystemRuntimeInterface` to the public deps for the runtime interactor)
- Create: `Source/ShinobiCoop/Tests/ShinobiOutfitTests.cpp`

**Interfaces:**
- Produces: `UShinobiOutfitBuilderLibrary` (`UBlueprintFunctionLibrary`, category `Shinobi|Outfit`, editor-only bodies under `WITH_EDITOR`, log and return false on failure, like `UShinobiWorldBuilderLibrary`):
  - `static bool BuildCloth(USkeletalMesh* Mesh, UPhysicsAsset* Collision, float HemMaxDistanceCm, float SleeveMaxDistanceCm)`. Creates one clothing asset from LOD 0 section 0 (`FClothingSystemEditorInterfaceModule::GetClothingAssetFactory()->CreateFromSkeletalMesh`), binds it to that section, and sets the `MaxDistance` weight map from the render vertex colours. R is the hem share (× `HemMaxDistanceCm`) and G the sleeve share (× `SleeveMaxDistanceCm`); the larger wins, matched to each physical vertex by nearest render vertex position. Sets `Collision` as the clothing physics asset, applies the masks, saves the mesh.
  - Default config values it writes into the asset's `UChaosClothConfig` (start values; Task 9 retunes them): `Density` 0.35, `EdgeStiffnessWeighted` 1.0, `BendingStiffnessWeighted` 0.5, `Damping` 0.1, `Drag` 0.07, `Lift` 0.07, `CollisionThickness` 1.0, `FrictionCoefficient` 0.8, `bUseSelfCollisions` false, `LinearVelocityScale` 0.6, `AngularVelocityScale` 0.6.

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Outfit.ClothSimulatesOnFollower` in `ShinobiOutfitTests.cpp`. Spawn a player as the Hero wearing Hokage (`SetCharacterIndex(0)`, `SetOutfitIndex(1)`), tick the world 1.0 s while adding forward movement input, then assert:
  - `GetOutfitMesh()->GetSkeletalMeshAsset()->GetMeshClothingAssets().Num() == 1`
  - `GetOutfitMesh()->GetLeaderPoseComponent() == GetHeroMesh()`
  - `GetOutfitMesh()->GetCurrentClothingData_GameThread().Num() > 0`
  - at least one simulated cloth vertex is more than 1 cm from its skinned position. Compare `ClothingData.Positions` against the bound section's skinned positions read through `USkinnedMeshComponent::GetSkinnedVertexPosition` for the mapped render vertices.
- [ ] **Step 2: Run it to make sure it fails:** `Scripts/build.ps1; Scripts/run_tests.ps1 -Filter ShinobiCoop.Outfit.ClothSimulatesOnFollower`. Expected: FAIL on the clothing-asset count (0).
- [ ] **Step 3: Implement `BuildCloth`.** For this gate only, call it from a throwaway Python snippet run through `Scripts/run_python.ps1` on `/Game/Shinobi/Outfits/Hokage/SK_Hokage`. Vertex colours aren't painted yet, so pass a debug path: when the mesh has no vertex colours, use a hem share of 1 below the hips bone's height, ramped over 30 cm. Collision is the Hero's physics asset (`/Game/Shinobi/Hero/SK_Hero` → `physics_asset`).
- [ ] **Step 4: Run the test.** Expected: PASS.
  - **If the follower doesn't simulate** (no clothing data under leader pose), stop. Report to the user with the log, and switch the later tasks to the Chaos Cloth Asset + Dataflow path, or to a follower without leader pose (Copy Pose From Mesh in a tiny AnimBP). Replan before continuing.
- [ ] **Step 5: Commit** `Build.cs`, the library and the test: `git commit -m "feat: editor-side cloth builder for outfits; cloth simulates on a leader-pose follower"`.

---

### Task 3: Body landmarks and tail detection

**Files:**
- Create: `Scripts/character/body_landmarks.py`, `Scripts/character/test_outfit_pipeline.py`

**Interfaces:**
- Produces (import with `sys.path.insert(0, os.path.dirname(__file__))`):
  - `load_body(fbx) -> (armature, mesh)`: factory-empty scene import, armature modifier removed, body facing -Y (turned like `skin_outfit.py` does).
  - `bone_head(armature, suffix) -> Vector`
  - `section(mesh, z, band=0.03) -> (width, depth, centre_x, centre_y)`: body only, hands and arms excluded by their strongest vertex group (`is_arm` / `is_hand`, as in `skin_outfit.py`).
  - `measure(fbx) -> dict`: keys `chest`, `belly`, `hips` (each `(width, depth)`), `shoulder_span`, `head_z`, `hips_z`, `knee_z`, `tail` (`None` or `{"root": (x, y, z), "tip": (x, y, z), "radius": r}`), in metres.
  - Tail: the largest connected body piece behind the back by more than 0.06 m below the hips bone (+Y is behind). Its capsule is the PCA axis through its vertices; the radius is the 80th percentile distance from the axis.
  - `POSES`: dict of pose name → list of `(bone_suffix, axis, degrees)`. `idle`: none. `arms_down`: `LeftArm`/`RightArm` about Y ∓50. `arms_out`: about Y ±50. `arms_up`: about Y ±120. `arms_forward`: about X −80. `jump_tuck`: `Left`/`RightUpLeg` about X −60, `Left`/`RightLeg` about X +120, arms about Y ±30.
  - `pose(armatures, name)`: applies `POSES[name]` to every armature given (body and cloak alike), each rotation in armature space as `M⁻¹ R M` on the pose bone. Verify each pose's direction on a render the first time: on 2026-10-19 the sign of "up" was found inverted.

- [ ] **Step 1: Write the failing test** in `test_outfit_pipeline.py` (a Blender script; asserts, then prints `PASS`):

```python
m = measure("SourceArt/Character/Models/Hero/Hero_rigged.fbx")
assert m["tail"] is None
assert 0.18 < m["chest"][1] < 0.28            # Hero chest depth ~0.23
c = measure("SourceArt/Character/Models/Cat/Cat_rigged.fbx")
assert c["tail"] is not None and c["tail"]["root"][1] > 0.05 and c["tail"]["tip"][2] < c["hips_z"]
b = measure("SourceArt/Character/Models/Bika/Bika_rigged.fbx")
assert b["chest"][1] > m["chest"][1] + 0.08   # Bika ~0.35 vs Hero ~0.23
print("PASS")
```

- [ ] **Step 2: Run it:** `blender -b --factory-startup -P Scripts/character/test_outfit_pipeline.py`. Expected: fails with `ModuleNotFoundError: body_landmarks`.
- [ ] **Step 3: Implement `body_landmarks.py`.** Print `measure()` for all five bodies; their tail results go into Task 4.
- [ ] **Step 4: Run the test.** Expected: `PASS`. Record which bodies have tails in the commit message.
- [ ] **Step 5: Commit:** `git commit -m "feat: body landmarks and tail detection for outfit fitting"`.

---

### Task 4: Per-body fitter (`skin_outfit.py`)

**Files:**
- Modify: `Scripts/character/skin_outfit.py`
- Modify: `Scripts/character/test_outfit_pipeline.py`

**Interfaces:**
- Consumes: `body_landmarks.measure`, `load_body`, `bone_head`.
- Produces: CLI `skin_outfit.py -- <body_rigged.fbx> <master.glb> <out_dir> <Outfit> <Body>` writes:
  - `<out_dir>/<Outfit>_<Body>_outfit.fbx`: skeleton plus the skinned copy, with vertex colours (R = hem share 0..1, G = sleeve share 0..1, B = 0) and UV channels 1 and 2 holding the master-space position (UV1 = (x, y), UV2 = (z, 0)), in metres, of the vertex in the master *before* fitting.
  - `<out_dir>/<Outfit>_outfit.png`: the texture, unchanged from today.
  - `<out_dir>/fit_<Body>_{front,side}.png`.
- The master-space UVs are what lets Task 8 copy cloth tuning between bodies.

Behaviour changes, each pinned by the test below:
1. **Per-body size.** Height still comes from the head and knee landmarks. Width and depth are scaled separately so the cloak's chest and hip sections are this body's `section()` plus 4 cm of girth (replaces the fixed `GIRTH`). The lift pass keeps its rest-position KD lookups.
2. **Hem weights:** hips and spine only (no thigh or leg groups), so the skinned target never stretches. The cloth swings it.
3. **Sleeve weights** as today (arm projection with the elbow blend).
4. **Colours:**
   - R ramps 0 → 1 from `hips_z + 0.05` down to the hem.
   - G ramps 0 → 1 along the sleeve from 40% of the shoulder-to-hand distance out to the cuff.
   - Both are 0 on the shoulders, chest, back and collar.
5. **Drop the rigid-skinning workarounds for the cloth regions:** the sleeve/coat split, weld slivers, armpit and skirt layer evening, seam splits and scrap removal. The cloth doesn't need them, and they made the holes. Keep the sleeve turn and the lift.
6. **Tail opening** when `measure()["tail"]` isn't `None`:
   - Delete the faces whose centre is within `radius + 0.03` m of the tail capsule axis, behind the coat (+Y).
   - Smooth the new boundary loop: 3 passes of `bmesh.ops.smooth_vert` on the boundary verts only.
   - Set its R share to 0.2 so the edge swings less.

- [ ] **Step 1: Write failing tests** in `test_outfit_pipeline.py`. Run the fitter for (Hokage, Hero) and (Hokage, Cat) into a temp dir, import each FBX, then assert:
  - every vertex has weights; no vertex has thigh or leg weight while `R > 0`
  - `max(R) == 1` and `max(G) == 1`; `R == G == 0` for vertices above `shoulder_z - 0.05` inside `±shoulder_span / 2`
  - UV channels 1 and 2 exist
  - on Cat the mesh has a boundary loop within 0.05 m of the tail root; on Hero there is none behind the hips
  - for 200 random body torso vertices of each body, the nearest cloak point is outward along the body normal (no torso vertex outside the coat)
- [ ] **Step 2: Run them.** Expected: FAIL (old CLI and outputs).
- [ ] **Step 3: Implement** the behaviour above in `skin_outfit.py`. Update its module docstring.
- [ ] **Step 4: Run the tests.** Expected: `PASS`. Also run the fitter for Akatsuki on Bika and open `fit_Bika_front.png` / `fit_Bika_side.png`: the coat must contain the torso.
- [ ] **Step 5: Commit:** `git commit -m "feat: per-body outfit fitter with cloth regions, master-space UVs and tail opening"`.

---

### Task 5: Hide mask baker

**Files:**
- Create: `Scripts/character/bake_hide_mask.py`
- Modify: `Scripts/character/test_outfit_pipeline.py`

**Interfaces:**
- Produces: CLI `bake_hide_mask.py -- <body_rigged.fbx> <Outfit>_<Body>_outfit.fbx <out.png>` writes an 8-bit greyscale PNG of 1024 × 1024 in the body's UV0 space. White (255) means show, black (0) means hide; bake margin 4 px.
- Rule: a body vertex is hidden only when it's enclosed. Rays from it along its normal and 8 directions 35° around the normal all hit the cloak within 0.20 m, in the rest pose *and* in each of `body_landmarks.POSES` (skinned cloak, cloth regions as skinned). A face is hidden when all its vertices are. Hands, head, neck above the collar, feet and the tail are never hidden. Then erode the hidden area by 2 cm along the surface so its edge stays under cloth.

- [ ] **Step 1: Write failing tests.** Bake (Hokage, Hero) and (Akatsuki, Hero), then assert:
  - chest-back vertices are hidden
  - Hokage's front-centre chest vertex at shoulder height − 0.25 m is shown (open front)
  - hand, forearm-at-wrist and neck-above-collar vertices are shown
  - shown share is between 25% and 75% of body vertices
- [ ] **Step 2: Run them.** Expected: FAIL (no script).
- [ ] **Step 3: Implement** with BVH ray casts. Write per-vertex 0/1 into a colour attribute, then bake it to the image (Cycles emit bake, 1 sample).
- [ ] **Step 4: Run the tests.** Expected: `PASS`.
- [ ] **Step 5: Commit:** `git commit -m "feat: bake per-body hide masks for outfits"`.

---

### Task 6: Posed acceptance check and the batch script

**Files:**
- Create: `Scripts/character/check_outfit.py`, `Scripts/character/build_outfits.ps1`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: `body_landmarks.POSES`, `pose`.
- Produces:
  - CLI `check_outfit.py -- <body_rigged.fbx> <outfit.fbx> <outfit.png> <hide.png> <out_prefix>` prints one line per pose: `POSE <name> stretched=<n> exposed=<n> poke=<n>`. It ends with `RESULT PASS` or `RESULT FAIL` and the exit code.
- Metrics, on the skinned (non-simulated) cloak:
  - `stretched`: cloak edges longer than 3× rest.
  - `exposed`: hidden body vertices whose normal ray escapes the cloak (see-through gaps).
  - `poke`: *shown* body vertices of the torso, upper arms and thighs that lie outside the cloak while the matching rest-pose vertex was inside.
- Thresholds per pose (start values, my call; record final ones in the outfit doc): `stretched ≤ 10`, `exposed ≤ 0.5%` of hidden vertices, `poke ≤ 20`.
- `build_outfits.ps1 [-Outfit <name>] [-Body <name>]`: for each outfit × body, runs `skin_outfit.py`, `bake_hide_mask.py` and `check_outfit.py`, and prints a table of `RESULT` lines. Exit 1 if any fail.
- `.gitignore`: keep `/SourceArt/Character/Outfits/*/*` ignored. Keep the `*_outfit.fbx` and `*_outfit.png` negations, add `!/SourceArt/Character/Outfits/*/*_hide.png`.

- [ ] **Step 1: Write the check script.** Run it on today's committed Hero Hokage (`Hokage_outfit.fbx` with an all-white mask). Expected: `RESULT FAIL` on `arms_up` and `jump_tuck` (the known stretching). This proves it catches the bug.
- [ ] **Step 2: Write `build_outfits.ps1` and run all ten pairs.** Expected: a table of ten rows. Fix the fitter and mask (Tasks 4–5) until all ten are `RESULT PASS`.
- [ ] **Step 3: Look at the renders** for every pair (`<out_prefix>_<pose>_{front,side,back}.png`): no visible body inside the cloak, finished tail openings. Note anything the metrics miss.
- [ ] **Step 4: Delete the old single copies:** `git rm SourceArt/Character/Outfits/*/{Hokage,Akatsuki}_outfit.fbx`. Their `.png` is now the shared texture and stays.
- [ ] **Step 5: Commit** the scripts, `.gitignore` and all ten `<Outfit>_<Body>_outfit.fbx` and `_hide.png`: `git commit -m "feat: posed outfit check; per-body Hokage and Akatsuki copies pass on all five bodies"`.

---

### Task 7: Game side. Per-body copy, hide mask, cloth reset, gravity, distance

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h/.cpp`
- Modify: `Source/ShinobiCoop/Tests/ShinobiOutfitTests.cpp`, `Source/ShinobiCoop/Tests/ShinobiHeroTests.cpp` (`OutfitFollowsBodyAndComesOff`: iterate per-body copies)

**Interfaces:**
- `FShinobiOutfitEntry` becomes `{ const TCHAR* Name; }`, with entry 0 `"None"`. Asset paths are derived:
  - `static FString OutfitMeshPath(int32 Outfit, int32 Body)` → `/Game/Shinobi/Outfits/<Outfit>/SK_<Outfit>_<Body>.SK_<Outfit>_<Body>`
  - `static FString OutfitHidePath(int32 Outfit, int32 Body)` → `/Game/Shinobi/Outfits/<Outfit>/T_<Outfit>_<Body>_Hide.T_<Outfit>_<Body>_Hide`
  - `<Body>` is `Roster[Body].Name`.
- `ApplyOutfit()`:
  - loads the copy for `CharacterIndex` (missing: wear nothing, `UE_LOG` Warning once per outfit/body pair)
  - sets every `HeroMesh` material slot to a dynamic instance with texture parameter `HideMask` = that texture (no outfit: `/Engine/EngineResources/WhiteSquareTexture`)
  - keeps the hips offset and leader pose
  - calls `OutfitMesh->ForceClothNextUpdateTeleportAndReset()`
- `void ResetOutfitCloth()`: public. Called from respawn, from every teleport in the codebase (find each `TeleportTo` / `SetActorLocation(..., ETeleportType::TeleportPhysics)` caller, for example Substitution), and from `ApplyOutfit`.
- Tick, owner-independent:
  - Gravity: when the character's gravity direction (`GetCharacterMovement()->GetGravityDirection()`) isn't world down, set each cloth interactor's gravity override (`UChaosClothingSimulationInteractor` / `UChaosClothingInteractor::SetGravity(1.f, true, GravityDirection * 981.f)`); otherwise clear the override.
  - Distance: suspend the cloth (`SuspendClothingSimulation` / `ResumeClothingSimulation`) when the component hasn't been rendered for 1 s, or is more than 40 m from the local player's camera. Constant `OutfitClothRangeCm = 4000.f`.

- [ ] **Step 1: Write failing tests** in `ShinobiOutfitTests.cpp` (Review Focus 1–5):
  - `SwapBodyKeepsFit`: wear Akatsuki as Hero, switch to Bika. The outfit mesh is `SK_Akatsuki_Bika`, the body's MIDs have `HideMask` = `T_Akatsuki_Bika_Hide`, and the outfit's leader pose is `HeroMesh`.
  - `TeleportResetsCloth`: after a 5000 cm teleport, the cloth positions are within 50 cm of the skinned ones on the next tick.
  - `ClothFollowsCharacterGravity`: set the character's gravity to +X. After 1 s, the hem cloth's mean is displaced along +X relative to the hips.
  - `MissingCopyWearsNothing`: point the roster at an unknown outfit/body pair through a test seam. `GetOutfitMesh()->IsVisible()` is false and the body mask is white.
  - `NoneRestoresBody`: after `SetOutfitIndex(0)`, every body MID's `HideMask` is the white texture.
- [ ] **Step 2: Run them:** `Scripts/build.ps1; Scripts/run_tests.ps1 -Filter ShinobiCoop.Outfit`. Expected: FAIL (assets and code missing). Task 8 provides the assets; run these tests again after it.
- [ ] **Step 3: Implement** the interfaces above. Also update `OutfitFollowsBodyAndComesOff` and `ShinobiCoop.Outfit.ClothSimulatesOnFollower` to the per-body paths.
- [ ] **Step 4: Build.** Expected: compiles. The tests pass after Task 8.
- [ ] **Step 5: Commit:** `git commit -m "feat: wear the outfit copy fitted to the current body, hide what it covers, reset and steer its cloth"`.

---

### Task 8: Import: `HideMask` material, ten copies, cloth, tail capsules, tuning propagation

**Files:**
- Modify: `Scripts/setup_hero.py`
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiOutfitBuilderLibrary.h/.cpp`

**Interfaces:**
- `UShinobiOutfitBuilderLibrary`, new and changed functions:
  - `BuildCloth(...)` (from Task 2): reads the R and G colours. The debug fallback is removed.
  - `static bool CopyClothTuning(USkeletalMesh* Source, const TArray<USkeletalMesh*>& Targets)`:
    - copies `Source`'s clothing configs onto each target (`DuplicateObject` per config class)
    - copies every enabled point weight map (MaxDistance, BackstopDistance, BackstopRadius, AnimDriveStiffness and the rest), value by value, from the source physical vertex nearest in master space. Master space comes from the UV1/UV2 of the nearest render vertex on each mesh.
    - saves the targets.
  - `static bool AddTailCapsule(UPhysicsAsset* Physics, FName Bone, FVector Root, FVector Tip, float Radius)`: one `FKSphylElem` on that bone's body, positions in component space, for the cloth to collide with. It also collides in the ragdoll, which is accepted.
- `setup_hero.py`:
  - `build_character_material()`: add `TextureSampleParameter2D` `HideMask` (default `/Engine/EngineResources/WhiteSquareTexture`). Opacity mask = `BaseColor.A × HideMask.R`.
  - `build_outfits()`: for each `SourceArt/Character/Outfits/<Outfit>/<Outfit>_<Body>_outfit.fbx`:
    - import it as `/Game/Shinobi/Outfits/<Outfit>/SK_<Outfit>_<Body>` with vertex colours imported (`FBXImportUI.skeletal_mesh_import_data.vertex_color_import_option = REPLACE`)
    - import `<Outfit>_<Body>_hide.png` as `T_<Outfit>_<Body>_Hide` (sRGB off, no mips, `TC_GRAYSCALE`)
    - share `MI_<Outfit>_<Outfit>_outfit` across the five copies
    - call `BuildCloth` with the body's physics asset, `HemMaxDistanceCm` 25 and `SleeveMaxDistanceCm` 12. On a re-import, the tuned values then come back through `CopyClothTuning` from the source.
  - Bodies: after a tailed body's import, call `AddTailCapsule` with `measure()["tail"]` converted to cm. Hips bone; the values come from `body_landmarks.py` through a JSON written by `build_outfits.ps1` to `SourceArt/Character/Models/<Body>/<Body>_tail.json`, committed.
  - `SETUP_HERO_ONLY=cloth` with `SETUP_HERO_CLOTH_SOURCE=<Outfit>_<Body>`: runs only `CopyClothTuning` from that copy to the outfit's other four.
  - Delete the stale single assets `SK_Hokage` and `SK_Akatsuki` (and their `_Skeleton`).

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Outfit.EveryCopyHasCloth`: for each outfit × roster body, `SK_<Outfit>_<Body>` loads and has 1 clothing asset whose MaxDistance map has values > 20 (hem) and values == 0 (pinned); `T_<Outfit>_<Body>_Hide` loads. Expected before import: FAIL.
- [ ] **Step 2: Implement** the library functions and the `setup_hero.py` changes. Build with the editor closed: `Scripts/build.ps1`.
- [ ] **Step 3: Import:** `$env:SETUP_HERO_ONLY="outfits"; Scripts/run_python.ps1 -Script Scripts/setup_hero.py`, then the bodies' tail capsules with a full `setup_hero.py` run (it re-imports the bodies). Expected: the log lists ten `SK_<Outfit>_<Body>` and two tail capsules.
- [ ] **Step 4: Run** `Scripts/run_tests.ps1 -Filter ShinobiCoop.Outfit` and `-Filter ShinobiCoop.Hero`. Expected: all pass, including Task 7's tests.
- [ ] **Step 5: Commit** the scripts, library, tail JSONs and `Content/Shinobi/Outfits/**`, `Content/Shinobi/Characters/**/PhysicsAsset*`, `Content/Shinobi/Hero/**` uassets touched: `git commit -m "feat: import per-body outfit copies with cloth, hide masks and tail capsules; copy cloth tuning between bodies"`.

---

### Task 9: In-game look: screenshot director, default cloth values that pass

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiPlayerController.h/.cpp` (`RunOutfitShotTest`, flag `-ShinobiOutfitShot`, args `-ShinobiOutfitShot=<Outfit>`)
- Modify: `Scripts/shot_chakra.ps1` (`-Outfit <name>` switch, pattern `ShinobiOutfitShot_*.png`)
- Modify: `Source/ShinobiCoop/Shinobi/Hero/ShinobiOutfitBuilderLibrary.cpp` (default config values, if retuned)

**Interfaces:**
- `RunOutfitShotTest()`, using the `At` / `Shot` / `Orbit` helpers as in `RunSusanooShotTest`. For each roster body (`ShinobiCharacter <Body>` then `ShinobiOutfit <Outfit>`), filmed from front (yaw 0), side (90) and behind (180) at 350 cm, aimed at the chest:
  1. idle after 2 s settle
  2. walking (`AddMovementInput` 0.4 for 1.5 s)
  3. running (1.0 for 1.5 s)
  4. mid-jump (`Jump()`, shot at 0.35 s)
  5. idle again 2 s after landing (settle check)

  Shot names: `ShinobiOutfitShot_<Outfit>_<Body>_<Phase>_<View>`. Quit when done.

- [ ] **Step 1: Implement the director and the switch.** Run `Scripts/shot_chakra.ps1 -Outfit Hokage` and `-Outfit Akatsuki`. Expected: 75 PNGs per outfit (5 bodies × 5 phases × 3 views).
- [ ] **Step 2: Review every shot** against the anti-goals:
  - no stretched strands
  - no holes
  - no body visible where hidden
  - no background visible through hidden parts
  - hem and sleeves trail in walk, run and jump
  - the final idle matches the first idle
- [ ] **Step 3: Retune the defaults** in `BuildCloth` and the `HemMaxDistanceCm` / `SleeveMaxDistanceCm` values. Re-import (`SETUP_HERO_ONLY=outfits`), re-shoot, until Step 2 holds for all ten pairs. Record the final values in the library's header comment.
- [ ] **Step 4: Run** `Scripts/run_tests.ps1 -Filter ShinobiCoop.Outfit`. Expected: PASS.
- [ ] **Step 5: Commit:** `git commit -m "feat: outfit screenshot director; default cloth values pass on every body"`.

---

### Task 10: The user's guide, memory, full suite

**Files:**
- Modify: `docs/character/outfit-steps.md`

- [ ] **Step 1: Rewrite `outfit-steps.md`** for the new pipeline: Tripo → `prep_outfit.py` (→ `fix_cloak_clouds.py`) → `build_outfits.ps1` → `setup_hero.py` → register in `Outfits` → test. Include the check thresholds and a **"Tuning the cloth"** section with numbered steps for the user:
  1. Close the game. Open the editor. In the Content Browser, open `/Game/Shinobi/Outfits/<Outfit>/SK_<Outfit>_Hero`.
  2. **Window → Clothing.** Select the clothing asset in the Clothing panel.
  3. In the toolbar, **Activate Cloth Paint**. In **Cloth Painting**, set **Paint Value** and choose the **Max Distance** map. A value is how far (cm) a vertex may swing from the body; 0 pins it.
  4. Paint with the brush, **Fill** or **Smooth**. Shift-drag erases to 0.
  5. **Preview:** press **Play** in the viewport toolbar, pick a walk or jump animation in the **Preview Scene Settings** tab, and watch the hem and sleeves.
  6. **Stiffness and swing:** in the **Clothing** panel's **Config** (Chaos Cloth Config) set Edge Stiffness, Bending Stiffness, Damping, Drag, Linear/Angular Velocity Scale. One line on what each does, with today's defaults.
  7. **Deactivate Cloth Paint** and save.
  8. Close the editor and copy the tuning to the other four bodies: `$env:SETUP_HERO_ONLY="cloth"; $env:SETUP_HERO_CLOTH_SOURCE="<Outfit>_Hero"; Scripts/run_python.ps1 -Script Scripts/setup_hero.py`.
  9. Check in game with `Scripts/shot_chakra.ps1 -Outfit <Outfit>`.

  Also cover the Limits section (one outfit slot; shirts and jumpers designed for but untried) and remove the old limits that no longer hold.
- [ ] **Step 2: Update** the memory file `character-asset-pipeline.md` (outside the repo) with the new pipeline and its gotchas, one paragraph.
- [ ] **Step 3: Run the full suite:** `Scripts/run_tests.ps1`. Expected: all pass. Report any failures with their output.
- [ ] **Step 4: Commit:** `git commit -m "docs: outfit pipeline with per-body copies, cloth and tuning steps"`.
