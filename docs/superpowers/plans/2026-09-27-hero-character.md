# Hero Character (Sub-project #2): Lean Implementation Plan

> **For agentic workers:** lean process per the user's standing preference (M2b–M5). Inline execution (native), one light check per milestone, the user's playtest at the end. Steps use checkbox (`- [ ]`) syntax.

**Goal:** replace the Manny player model with an anime-likeness hero generated from the user's photos. All existing animations are driven at runtime from a hidden Manny.

**Architecture:**
- A Higgsfield reference and image-to-3D.
- A headless Blender prep script.
- The user's Mixamo auto-rig.
- A UE import script that builds the mesh, physics, material, IK rig and retargeter.
- A native `UShinobiRetargetAnimInstance` (running `FAnimNode_RetargetPoseFromMesh`) on a `HeroMesh` component attached to the hidden driver mesh.

**Tech Stack:** UE 5.8 C++ (IKRig plugin), UE Python commandlet, Blender 5.2 Python (`blender -b -P`), Higgsfield MCP, Mixamo (manual).

**Spec:** `docs/superpowers/specs/2026-09-27-hero-character-design.md`

## Global Constraints

- Photos, references, raw GLBs and `hero_prep.fbx` never enter git (already in `.gitignore`). Only `SourceArt/Character/Mesh/hero_rigged.fbx` and `/Game/Shinobi/Hero/*` are committed.
- Uploads only to Higgsfield (stages 1–3). The Mixamo upload is done by the user.
- Hero height 180 cm, feet at the origin, facing -Y (Blender front). ≤ 60,000 triangles, one material.
- The user picks at every generation checkpoint. After 3 bad image-to-3D tries, stop and present options.
- The cvar `Shinobi.Hero` (default 1). With 0, or with the assets missing, you get today's visible Manny.
- Enemies and dummies stay Manny. Every player is the hero. Teammates get a floating name tag.
- Keep the user's uncommitted `Config/DefaultEngine.ini` map edit out of commits. Stage by explicit path. Close the editor before builds and scripts.

## Review Focus

1. **A player that is downed, then dead, then respawned:** the new pawn gets a fresh `HeroMesh`, and the ragdolled old body is destroyed with it. Test: `HeroSurvivesRespawn`, Task 3.
2. **A client joining mid-wave:** remote players' `HeroMesh` animates from their replicated driver. Covered by the smoke test plus playtest row 2.
3. **The cast orb and aura while the hero is hidden in first frames:** `GetBodyMesh()` must never return null. Test: `BodyMeshFallsBackToDriver`, Task 3.
4. **Hit-stop on a hero:** freezing the driver's `GlobalAnimRateScale` freezes the hero too. Check by screenshot in Task 4.
5. **Name tag on the local player:** it must not show. Test: `NameTagOnlyOnTeammates`, Task 3.

---

### Task 1 (C1): Anime A-pose reference

- [ ] Upload the 3 photos (`SourceArt/Character/Photos/*.jpeg`) to Higgsfield. Generate ~4 full-body **front** views:
  - an anime (Naruto-style) likeness of the person, in a shinobi outfit with a forehead protector;
  - A-pose with arms ~45° down, hands open, neutral face;
  - plain light background, no props.
  Save them as `SourceArt/Character/Reference/front_<n>.png`.
- [ ] The user picks one, or asks for another round.
- [ ] Generate a matching **back** view from the pick and save it as `Reference/back.png`. Quick check with the user.

### Task 2 (C2): Mesh + Blender prep

**Files:** create `Scripts/character/prep_mesh.py`. Outputs go to `SourceArt/Character/Mesh/`.
- [ ] Higgsfield `generate_3d` on the picked front (plus the back if it's supported). Save to `Mesh/raw_<n>.glb`. Up to 3 tries.
- [ ] `prep_mesh.py <in.glb> <out.fbx>`, run as `"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P Scripts/character/prep_mesh.py -- <in> <out>`:
  1. import and apply transforms;
  2. scale so the height is 180 cm, feet at z = 0, facing -Y;
  3. merge by distance (0.1 mm); delete loose parts under 1% of the vertex count;
  4. decimate to ≤ 60,000 triangles;
  5. one material, with the image texture packed;
  6. export FBX (embedded texture, `path_mode='COPY'`, `embed_textures=True`);
  7. render `preview_front.png` and `preview_side.png` (Workbench, textured).
  Exit code 1 with a message on: no mesh, > 60k triangles after decimation, or height outside 170–190 cm after scaling.
- [ ] Render turnarounds of each raw GLB for the user's pick. Prep the pick. Check the previews (Read tool).

### Task 3 (C4, part 1): Runtime hero plumbing (built against a stand-in)

Stand-in: `/Game/Shinobi/Mixamo/Power_Up_30fps` (a Mixamo-rigged mesh already in the project).

**Files:**
- Create `Source/ShinobiCoop/Shinobi/Hero/ShinobiRetargetAnimInstance.h/.cpp`.
- Modify `ShinobiCharacterBase.h/.cpp`, `ShinobiPlayerCharacter.h/.cpp`, `VFX/ShinobiChakraAuraComponent.cpp`, `VFX/ShinobiCastFxComponent.cpp`, `UI/ShinobiOverheadTextComponent.h/.cpp` and `ShinobiCoop.Build.cs` (add `IKRig`); `.uproject` (IKRig plugin if needed).
- Create `Scripts/setup_hero.py`.
- Test: `Source/ShinobiCoop/Tests/ShinobiHeroTests.cpp`.

**Interfaces:**
- `UShinobiRetargetAnimInstance : UAnimInstance`, with a custom proxy holding `FAnimNode_RetargetPoseFromMesh` (`bUseAttachedParent = true`, `IKRetargeterAsset`).
  - `static constexpr const TCHAR* RetargeterPath = TEXT("/Game/Shinobi/Hero/RTG_MannyToHero.RTG_MannyToHero")`.
  - Pattern: the engine's `IKRigEditor/Public/RetargetEditor/IKRetargetAnimInstance(Proxy).h`.
- `USkeletalMeshComponent* AShinobiCharacterBase::GetBodyMesh() const`: the `HeroMesh` if it's set, else `GetMesh()`. Never null.
- `FName AShinobiCharacterBase::GetRightHandBone() const`: `"hand_r"` for Manny, `"mixamorig:RightHand"` for the hero.
- `AShinobiPlayerCharacter`:
  - `UPROPERTY USkeletalMeshComponent* HeroMesh`, created in the ctor and attached to `GetMesh()`.
  - `ApplyHero()` in `BeginPlay`: loads `/Game/Shinobi/Hero/SK_Hero` and the retargeter. If both load and `Shinobi.Hero` = 1: set the mesh and anim class; the driver gets `SetVisibility(false)`, `SetCastShadow(false)` and `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones`. Else destroy `HeroMesh` and leave the driver as is.
- `UShinobiOverheadTextComponent::SetNameMode(const FString& Name)`: shows the name and skips the HP/status text.
- `setup_hero.py <fbx> <asset dir>` imports a Mixamo-rigged FBX as `SK_Hero` (+ skeleton + `PA_Hero` + `MI_Hero`) and builds `IK_Hero` (auto-characterised chains, as in `setup_chakra_pose.py`) and `RTG_MannyToHero` (source `IK_Manny`, auto-mapped chains). It asserts that every Manny limb chain maps.

**Steps:**
- [ ] Tests in `ShinobiHeroTests.cpp`:
  - `ShinobiCoop.Hero.PlayerUsesHeroWhenAssetsExist`: `HeroMesh` valid, anim class is `UShinobiRetargetAnimInstance`, driver `IsVisible() == false`, driver tick option `AlwaysTickPoseAndRefreshBones`.
  - `ShinobiCoop.Hero.CvarOffKeepsManny`: under a cvar guard `Shinobi.Hero` = 0, `HeroMesh` is null and the driver is visible.
  - `ShinobiCoop.Hero.BodyMeshFallsBackToDriver`: a dummy's `GetBodyMesh() == GetMesh()`.
  - `ShinobiCoop.Hero.DeadRagdollsBody`: apply `GE_Dead`, then the `GetBodyMesh()` of `IsSimulatingPhysics()` is true.
  - `ShinobiCoop.Hero.HeroSurvivesRespawn`: destroy and respawn a player pawn; the new pawn has a valid `HeroMesh`.
  - `ShinobiCoop.Hero.NameTagOnlyOnTeammates`: covered by logic in `SetNameMode` callers; the test asserts that a player whose `IsLocallyControlled()` is true has a hidden name tag.
- [ ] Build `RTG_MannyToHero` / `SK_Hero` from the **stand-in**: `setup_hero.py` with `SourceArt/Mixamo/Power_Up_30fps.fbx`.
- [ ] Implement, build, and run the tests: all pass (56+).
- [ ] Route the aura overlay (`ChakraAura`), the cast orb attach (`CastFx`, via `GetRightHandBone()`) and `OnDeadTagChanged` through `GetBodyMesh()`.
- [ ] Name tags: `AShinobiPlayerCharacter` gets an overhead text in name mode, hidden when locally controlled (updated in `Tick`, since possession can arrive after `BeginPlay`).
- [ ] Screenshots (`shot_chakra.ps1`, `-Jutsu`, `-Look`) with the stand-in: pose follows, orb in hand, aura on the body. Run `smoke_m1` and `smoke_m4`. Commit.

### Task 4 (C3 + C4, part 2): Real hero

- [ ] Write `docs/character/mixamo-steps.md`:
  1. upload `hero_prep.fbx`;
  2. place the markers (chin, wrists, elbows, knees, groin), symmetry on;
  3. skeleton LOD "Standard (65)";
  4. download FBX Binary, With Skin, no animation (T-pose off / "Original Pose");
  5. save as `SourceArt/Character/Mesh/hero_rigged.fbx`.
- [ ] The user does the Mixamo step.
- [ ] Run `setup_hero.py` with `hero_rigged.fbx` (it replaces the stand-in assets in place). Tune the IK chains and retarget pose until the screenshots show clean feet, wrists and shoulders.
- [ ] Hit-stop check (a mid-hit screenshot), downed pose, ragdoll. Run all tests and both smoke tests. Commit `hero_rigged.fbx` and `/Game/Shinobi/Hero`.
- [ ] User playtest (2 players):
  1. hero in all states;
  2. partner sees the hero animate;
  3. name tags on teammates only;
  4. `Shinobi.Hero 0` gives Manny back.

## Progress notes

- **2026-09-27, C1:** Higgsfield produced 3 front candidates (`front_1..3.png`, local only). They cost 6 credits: the `get_cost` preflight of 2 was per image, not per batch, and the batch returned 3 images, not 4. The balance is now 0.
- **Pick:** candidate 1 (round glasses, black curly hair), but in an Akatsuki-style cloak.
- **Deviation from the spec:**
  - The user makes the cloak edit (free Gemini) and runs the 3D step on a free web tool (Meshy / Tripo) themselves.
  - Inputs: `Reference/front_cloak.png`, then `Mesh/raw_1.glb`.
  - The cloak is requested open and knee-length, with a clear leg gap, so image-to-3D and Mixamo don't fuse the legs.
- **IP note:** the Leaf headband symbol and the Akatsuki cloak design are Naruto's. Both must be replaced before any public release.
- **In parallel:** Task 3 (runtime plumbing against the stand-in).
- **2026-09-27, C2:** the user made `front_cloak.png` (open, knee-length Akatsuki-style cloak). Image-to-3D ran on Hyper3D Rodin Gen-2.5 Medium through the user's authenticated `hyper3d` MCP, not Meshy (Claude can't sign in to the user's Meshy account). Settings: Raw mesh, 50k polygons, GLB.
- **C2 result:** `Mesh/raw_1.glb` (PBR texture); `raw_1_shaded.glb` (baked lighting) is kept as a spare. `prep_mesh.py` produced one part, no specks, 50,000 triangles, 180.0 cm. The previews look clean: separate fingers, a leg gap, glasses intact. Next is C3 (the user's Mixamo step).
- **2026-09-27, C3 + C4:** the user rigged `hero_prep.fbx` on Mixamo (Standard Skeleton, original pose): `hero_rigged.fbx`. `setup_hero.py` imported it: SK_Hero, its physics asset, M_Hero plus T_Hero_Diffuse, and all 7 body chains mapped. Fixes found along the way:
  - Mixamo packs textures without a file extension, which Interchange can't read. `extract_textures.py` writes `hero_rigged.png`.
  - Material slot structs had to be rebuilt; editing the returned copies did nothing.
  - The cast orb uses absolute scale because the rig's hand bone is scaled 100x.
- **C4 checks:** screenshots show idle (facing correctly, textured), chakra aura on the hero, the Chidori and Rasengan orbs in the right hand, and the Training Grounds fight. 56 tests and both smoke tests pass. Waiting on the user's playtest (rows above).

**Result (2026-09-27):** the user ran the playtest; all rows pass ("all good").
