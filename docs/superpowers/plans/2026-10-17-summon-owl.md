# Summon Owl Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Summon Toad (page 4, slot 2) with a plush-sized snowy owl who hovers at the caster's left shoulder and calls no-stagger lightning down from the sky onto the nearest visible enemy.

**Architecture:** The owl is her own lightweight replicated actor, `AShinobiOwl` (skeletal mesh, no collision, no AI, no health), not an `AShinobiSummon`. Every machine moves her to a post computed from the replicated caster; the server picks targets on a timer and applies damage through a new no-stagger damage effect. A new `EShinobiBurst::SkyBolt` draws the bolt. The model is the Tripo FBX re-weighted and keyed in Blender by `build_owl.py`.

**Tech Stack:** UE 5.8 C++ (GAS), UE automation tests, Blender 5.2 headless Python, UE editor Python import (`setup_creatures.py`, `setup_hud.py`), SpriteCook (icon).

**Spec:** `docs/superpowers/specs/2026-10-17-summon-owl-design.md`

## Global Constraints

- Work only in the worktree `E:\ShinobiCoop-owl`, branch `feature/summon-owl`. Never touch `E:\ShinobiCoop` (someone else's uncommitted Tairn work on `feature/tripo-tairn`), except to copy the owl source files out of it in Task 3.
- Close the Unreal Editor before `Scripts/build.ps1`, `Scripts/run_tests.ps1` or `Scripts/run_python.ps1`. Never switch or merge branches with UE open.
- Tests: during the work run only the groups a task names (`powershell -File Scripts/run_tests.ps1 -Filter ShinobiCoop.Summons`, `... -Filter ShinobiCoop.Pvp`, `... -Filter ShinobiCoop.Damage` etc.). The full suite (`-Filter ShinobiCoop`) runs once, in Task 7 (the user's standing rule).
- Owl numbers (spec): lifetime 15 s; strike every 2.0 s; 55 damage; strike range 2000 cm from the caster; bolt lands 0.4 s after the strike starts; chakra 60; cooldown 45 s; 45 cm tall (model scale 0.45); post 60 cm left, 30 cm behind, 160 cm above the caster's feet.
- Her damage never staggers or shoves and applies no status (no stun, no shock bonus).
- She/her in comments and docs. Comments follow the surrounding style: plain sentences, what and why.
- Do not take desktop screenshots for game shots (they capture the user's browser).
- SpriteCook: check the balance and ask the user before spending credits.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. The caster dies, is downed or leaves mid-strike (bolt pending): the bolt must not land and nothing may crash; she leaves in a poof. Test in Task 4 (`OwlLeavesWhenHerCasterFalls`, cast then kill the caster 0.2 s after a strike starts).
2. The target dies or becomes invulnerable (dodge, Kamui) during the 0.4 s wind-up: no damage to a dead target, no crash; an invulnerable one is handled by `ApplyDamage` as usual. Test in Task 4 (`OwlBoltSparesATargetThatFellMeanwhile`).
3. A client (not the server) sees her: she must follow the caster smoothly without replicated movement and play Strike when the server strikes. No automated test (the test world is standalone): checked in Task 7's two-player PIE run.
4. Casting her twice, or casting with Misha and Pabu out: exactly one owl per caster, ground summons untouched, another caster's owl untouched. Test in Task 4 (`OwlReplacesHerselfOnly`).
5. PvP: she strikes enemy-team players, never her caster's teammates. Test in Task 4 (`OwlStrikesTheCastersFoesInPvp`).

---

### Task 1: Damage that does not stagger

**Files:**
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiEffects.h` / `.cpp` (new `UGE_DamageNoStagger`)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiDamageExec.cpp` (and its header comment)
- Modify: `Source/ShinobiCoop/Shinobi/ShinobiCombat.h` / `.cpp`
- Test: the existing damage test file (find it: `grep -l "ShinobiCoop.Damage" Source/ShinobiCoop/Tests/*.cpp`)

**Interfaces:**
- Produces: `bool ShinobiCombat::ApplyDamage(UAbilitySystemComponent* Source, const AShinobiCharacterBase* Target, float Amount, bool bStagger = true)`; `UGE_DamageNoStagger : public UGE_Damage` (same SetByCaller `Data.Damage`).

- [ ] **Step 1: Write the failing test** `ShinobiCoop.Damage.NoStaggerHitLeavesAJutsuRunning`: a player on page 4 taps `Jutsu3` (Summon Misha, 0.7 s wind-up); immediately `ApplyDamage(OtherASC, P, 55.f, false)`; assert `ShinobiTest::IsActive(P, UGA_SummonMisha::StaticClass())`, health dropped by 55, and `!HasTag(P, ShinobiTags::State_Staggered)`. Second half, a fresh player: same with `bStagger = true` → the jutsu is no longer active.
- [ ] **Step 2: Build and run** `-Filter ShinobiCoop.Damage`. Expected: compile error (no 4th parameter).
- [ ] **Step 3: Implement.** `ApplyDamage` applies `UGE_DamageNoStagger` when `!bStagger`. In the exec, `const bool bNoStagger = Spec.Def && Spec.Def->IsA<UGE_DamageNoStagger>();` skips both the `Event_Hit_Stagger` send and the physical-reaction shove. Everything else (Susanoo, boosts, invulnerable, shock-bonus consumption, block, shield) unchanged. Update the exec header's rule list.
- [ ] **Step 4: Run** `-Filter ShinobiCoop.Damage`. Expected: all pass.
- [ ] **Step 5: Commit** `feat: damage that does not stagger`.

### Task 2: Retire the toad

**Files:**
- Modify: `Shinobi/Enemies/ShinobiSummon.h/.cpp`, `Shinobi/Enemies/GA_SummonAttacks.h/.cpp`, `Shinobi/Abilities/GA_Summons.h/.cpp`, `Shinobi/ShinobiTags.h/.cpp`, `Shinobi/ShinobiPlayerCharacter.cpp:79`, `Shinobi/ShinobiPlayerController.cpp` (debug cast ~390, shot timeline ~901-904), `Scripts/setup_creatures.py` (Toad row), `Scripts/setup_hud.py` (toad icon row)
- Test: `Tests/ShinobiSummonTests.cpp`, `Tests/ShinobiPvpTests.cpp`

**Interfaces:**
- Produces: `enum class EShinobiSummonKind : uint8 { Misha, Pabu };` (stats and look tables reordered to match; the `Kind` default and `UGA_SummonBase::SummonKind` default become `Misha`). Page 4 slot 2 is empty until Task 4.

- [ ] **Step 1: Convert the tests.** Delete `ToadSlamHitsEveryoneAroundIt`, `ToadIsTheRiggedToadThatHopsAndSlams`, `ToadJutsuCostsChakraThenCallsTheToad`. In every other test (Summons and PvP) a toad becomes Misha, with Misha's numbers (health 500, damage 70, range 700, interval 2.2 s, speed 380, cost 50, `Jutsu3`); `AppearInFrontOfAWallNotInsideIt` taps `Jutsu3` and asserts Misha's clearance (her capsule radius 110). `OneOfEachKindPerCaster` uses Misha then Pabu. Drop the toad from `OnlyCallableFromPageFour` and `JutsuHaveSealAnimationsAndIcons` (the owl joins them in Task 4/6).
- [ ] **Step 2: Remove the toad code:** `EShinobiSummonKind::Toad`, its stats/look rows, `UGA_ToadSlam`, `UGA_SummonToad`, `Cooldown_Jutsu_SummonToad`, the toad `FollowAngle` branch, the toad entry in the player's ability list, the controller's toad debug cast (delete the line) and the toad shot lines 901-904 (delete; Task 5 adds the owl's), the `"Toad"` row in `setup_creatures.py` and `jutsu_summontoad.png` in `setup_hud.py`. Keep `build_toad.py`, `SourceArt/Creatures/Toad` and `/Game/Shinobi/Creatures/Toad` (note in the `CREATURES` comment that the toad is retired, like the snake).
- [ ] **Step 3: Verify no toad is left** in gameplay code: `grep -rn -i toad Source Scripts/setup_*.py` shows only the retired-asset comment.
- [ ] **Step 4: Build and run** `-Filter ShinobiCoop.Summons` and `-Filter ShinobiCoop.Pvp`. Expected: all pass.
- [ ] **Step 5: Commit** `refactor: retire the summoned toad`.

### Task 3: The owl model and clips (Blender)

**Files:**
- Copy in: `E:\ShinobiCoop\SourceArt\Tripo\Owl\{IMG_9900-9902.jpeg, owl_front/left/back.jpeg, owl_humanoid_rigged.fbx, owl_humanoid_rigged.glb}` → `SourceArt/Tripo/Owl/` (commit them; skip the walk/jump FBX)
- Create: `Scripts/character/build_owl.py`
- Output: `SourceArt/Creatures/Owl/Owl.fbx`, `Owl_Hover.fbx`, `Owl_Strike.fbx`, `Owl.png`
- Modify: `Scripts/setup_creatures.py` (row `"Owl": (None, ["Hover", "Strike"], "Owl.png")`)

**Interfaces:**
- Produces (UE): `/Game/Shinobi/Creatures/Owl/SK_Owl`, `ANIM_Owl_Hover` (seamless loop), `ANIM_Owl_Strike` (starts and ends in Hover's frame-0 pose, the down-beat at 0.4 s). Model faces +X, feet's lowest point at Z = 0, true model size (1.0 m tall); the game scales it by 0.45.

Facts: the FBX has the mesh and 46 Mixamo-named joints (`mixamorig:Hips`, `Spine/Spine1/Spine2`, `Neck`, `Head`, `Left/RightShoulder`, `Arm`, `ForeArm`, `Hand` plus finger joints, legs to `ToeBase`), no texture. The texture is only in the GLB: `images[0]` "owl_humanoid_rigged_basecolor.jpg" (base colour only, no normal or roughness map). Read it with plain `struct` + `json` from the GLB's BIN chunk; do not import the GLB into Blender (NaN bones). The GLB is Y-up, wings along ±X, 0.89 m wide.

- [ ] **Step 1: Write `build_owl.py`** on the structure of `build_misha.py` (docstring with run line, `-- preview [Clip...]` and `-- rig` modes, `OUT`, `FPS = 30`, `GAME_SCALE = 0.45`). Import the FBX; turn it to face +X, feet on Z = 0; delete the finger joints; re-weight by bone heat (Tripo's weights are broken); hand over any unweighted or torn verts the way `build_misha.py`'s `hand_over` does. Wing = Arm/ForeArm/Hand (root/mid/tip). Write `Owl.png` (2048², from the GLB JPG). Keys: **Hover** 0.5 s loop: wings beat about ±35° at the shoulder with the forearm and hand lagging behind, the body bobs 3 cm, legs hang down and back a little; **Strike** 0.8 s: wings sweep up and hold, head tips up, hard down-beat landing at frame 12 (0.4 s), back to Hover's frame 0 by the end. Export via `export_creature.py` like Misha.
- [ ] **Step 2: Run** `blender -b -P Scripts/character/build_owl.py -- rig` and `... -- preview Hover Strike`. Check the renders: no torn wing or stretched belly, the plush texture lands where it should (eyes, beak, spots), the feet hang.
- [ ] **Step 3: Approval checkpoint.** Show the user the preview renders (paths under `Saved/`) and wait for approval or changes before importing.
- [ ] **Step 4: Import:** add the `CREATURES` row, then with UE closed `powershell -File Scripts/run_python.ps1 Scripts/setup_creatures.py` with `CREATURES_ONLY=Owl` set. Expected: `SK_Owl`, `ANIM_Owl_Hover`, `ANIM_Owl_Strike` in `/Game/Shinobi/Creatures/Owl`.
- [ ] **Step 5: Commit** the script, source art, `SourceArt/Creatures/Owl/*` and `Content/Shinobi/Creatures/Owl/*`: `feat: the owl plush, rigged and keyed for hovering`.

### Task 4: `AShinobiOwl` and Summon Owl

**Files:**
- Create: `Source/ShinobiCoop/Shinobi/Enemies/ShinobiOwl.h/.cpp`
- Modify: `Shinobi/Abilities/GA_Summons.h/.cpp` (`UGA_SummonOwl`), `Shinobi/ShinobiTags.h/.cpp` (`Cooldown_Jutsu_SummonOwl` "Cooldown.Jutsu.SummonOwl"), `Shinobi/ShinobiPlayerCharacter.cpp:79`
- Test: `Tests/ShinobiSummonTests.cpp`

**Interfaces:**
- Consumes: `ShinobiCombat::ApplyDamage(..., false)` (Task 1); `SK_Owl`, `ANIM_Owl_Hover`, `ANIM_Owl_Strike` (Task 3); `EShinobiBurst::SkyBolt` comes in Task 5, so here the bolt spawns `EShinobiBurst::Lightning` at the target's feet.
- Produces:
  ```cpp
  UCLASS() class SHINOBICOOP_API AShinobiOwl : public AActor {
  public:
      static AShinobiOwl* Summon(AShinobiCharacterBase* Caster); // server only; dismisses Caster's previous owl; null on a client
      static FVector GetPost(const AShinobiCharacterBase& Caster); // world point her body centre hovers at
      AShinobiCharacterBase* GetSummoner() const;
      AShinobiCharacterBase* GetTarget() const;
      void Dismiss(); // server: poof (SummonPoof, scale 0.3) and destroy
      static constexpr float Lifetime = 15.f, FirstStrikeDelay = 0.5f, StrikeInterval = 2.f, WindUp = 0.4f, Damage = 55.f, StrikeRange = 2000.f, Scale = 0.45f;
      static constexpr float PostBehind = 30.f, PostLeft = 60.f, PostHeight = 160.f; // PostHeight above the caster's feet
  };
  ```
  `UGA_SummonOwl` (page 4, `Jutsu2`, "Summon Owl", icon path `/Game/Shinobi/UI/T_Jutsu_SummonOwl.T_Jutsu_SummonOwl`, chakra 60, cooldown 45, `Cooldown_Jutsu_SummonOwl`) overrides `ExecuteJutsu` to call `AShinobiOwl::Summon(Self)` then `EndJutsu()`.

Behaviour the tests pin: replicates (`bReplicates`, movement not replicated); `Summoner` replicated initial-only, `Target` replicated, a `ReplicatedUsing` strike counter that plays Strike on clients. Tick on every machine: `VInterpTo` towards `GetPost(Caster)` (speed about 8; farther than 1500 from it, e.g. after a teleport or on a dragon, she snaps to it), yaw towards `Target` while striking, else the caster's yaw; Hover loops again once Strike finishes (single-node animation; skip the mesh on a dedicated server, like `WearCreature`). Server: dismiss when the caster is null or `!IsAlive()`; lifetime timer; strike timer (first after `FirstStrikeDelay`, then every `StrikeInterval`) picks the nearest of `ShinobiCombat::FindHostilesInRadius(Caster, Caster location, StrikeRange)` that she can see (an `ECC_Visibility` trace from her to the target, ignoring her and the caster, clear or hitting the target), bumps the counter, and `WindUp` later lands the bolt on that target if the caster and target are still alive. Root and mesh have no collision.

- [ ] **Step 1: Write the failing tests** (positions in cm, caster at the origin facing +X, everyone `ShinobiTest::Hover`ed):
  - `OwlJutsuCostsChakraThenCallsTheOwl`: page 4, tap `Jutsu2` → chakra 40; tick 1.2 → `Cooldown_Jutsu_SummonOwl` present, `UGA_SummonOwl` not active, `CountActors<AShinobiOwl> == 1`, `CountActors<AShinobiSummon> == 0`, `GetSummoner() == P`.
  - `OwlReplacesHerselfOnly`: Misha and Pabu out, summon the owl twice → 1 owl, 2 ground summons; another caster's owl → 2 owls.
  - `OwlHoversAtTheLeftShoulder`: `GetPost` for the caster: Y in [-70, -50], X in [-40, -20], Z = feet + 160 ± 5; after tick 1.0 she is within 20 of it; caster teleported to (800, 0, 0) and turned to yaw 90, tick 1.0 → within 20 of the new post, which is now at X ≈ 800 + 60 (left of a caster facing +Y); caster teleported to (6000, 0, 0), one tick → within 20 of the post (she snaps).
  - `OwlHasNoBodyToHit`: her root and mesh `GetCollisionEnabled() == NoCollision`; `FindHostilesInRadius(Ninja, Owl location, 500)` is empty of her (she is not a character).
  - `OwlStrikesTheNearestEnemyInSight`: ninjas at (800, 0) and (1500, 0); tick 1.0 → the near one lost exactly 55, the far one nothing, neither has `State_Staggered`; tick 2.0 more → the near one lost 110 total.
  - `OwlIgnoresEnemiesOutOfRangeOrBehindWalls`: a ninja at (2500, 0) and one at (0, 800) behind a BlockAll cube wall at (0, 400) (the wall test's cube setup) → after 3.0 s neither hurt.
  - `OwlBoltSparesATargetThatFellMeanwhile`: ninja at (800, 0); tick 0.6 (strike started), kill the ninja (health to 0 the way other tests do), tick 0.5 → no crash, ninja health unchanged by her.
  - `OwlLeavesWhenHerTimeIsUp`: tick 14.5 → 1 owl; tick 1.0 → 0. `OwlLeavesWhenHerCasterFalls`: a ninja at (800, 0); tick 0.6, down the caster (as `LeaveWhenTheirCasterFalls` does), tick 0.5 → 0 owls and the ninja untouched.
  - `OwlStrikesTheCastersFoesInPvp` (in `ShinobiPvpTests.cpp`, on that file's team set-up): caster on team A, a team-A player and a team-B player both at 800 → after 1.0 s only the team-B player lost 55.
  - `OwlWearsThePlushAndHovers`: `SK_Owl` is her mesh at scale 0.45, Hover playing; after a strike starts, Strike playing; 1.0 s later Hover again.
  - Add the owl to `OnlyCallableFromPageFour` (`Cooldown_Jutsu_SummonOwl`, `UGA_SummonOwl`, `CountActors<AShinobiOwl> == 0`).
- [ ] **Step 2: Build and run** `-Filter ShinobiCoop.Summons`. Expected: compile errors (no `AShinobiOwl`).
- [ ] **Step 3: Implement** `AShinobiOwl`, `UGA_SummonOwl`, the tag, and the player's ability list (`UGA_SummonOwl` where the toad was).
- [ ] **Step 4: Run** `-Filter ShinobiCoop.Summons` and `-Filter ShinobiCoop.Pvp`. Expected: all pass.
- [ ] **Step 5: Commit** `feat: Summon Owl, a plush owl who calls lightning at the caster's shoulder`.

### Task 5: The sky bolt and the shot timeline

**Files:**
- Modify: `Shinobi/VFX/ShinobiBurst.h/.cpp` (append `SkyBolt` to `EShinobiBurst`, after `SummonPoof`), `Shinobi/Enemies/ShinobiOwl.cpp` (use it), `Shinobi/ShinobiPlayerController.cpp`

**Interfaces:**
- Produces: `EShinobiBurst::SkyBolt`: spawned at the target's feet; draws Chidori-blue `ESprite::Lightning` cards stacked from the ground up to about 1500 above it (a few cards, flickering, about 260 wide), a white core, and the `Lightning` burst's flash and light at the ground; 0.25 s.

- [ ] **Step 1: Implement** the `SkyBolt` case beside `Lightning` in `ShinobiBurst.cpp` (`AddCard` with `Offset` Z rising per card). The owl's landing spawns `SkyBolt` instead of `Lightning`. If `ShinobiSfx` has a thunder or lightning sound, play it at the impact; otherwise none.
- [ ] **Step 2: Shot timeline:** in the controller's jutsu-shot sequence put the owl where the toad's shots were (cast `Jutsu2` at 52.0, shots `ShinobiJutsuShot_28_Owl` and `_28b_Owl`, with a ninja in strike range if the sequence has one, otherwise just her hovering); the debug follow sequence casts slot 2 again (`// Owl`); `ClearSummons` also dismisses every `AShinobiOwl`.
- [ ] **Step 3: Look at it:** play in the editor (or the shot script, if page 4 is reachable there; it was not on some branches) and check the bolt reads as coming from the sky and she sits clear of the camera. Adjust the card sizes and the post if not.
- [ ] **Step 4: Run** `-Filter ShinobiCoop.Summons`. Expected: all pass.
- [ ] **Step 5: Commit** `feat: the owl's lightning falls from the sky`.

### Task 6: The icon

**Files:**
- Create: the icon PNG where `setup_hud.py` reads the others (`jutsu_summonowl.png`)
- Modify: `Scripts/setup_hud.py` (`"jutsu_summonowl.png": ("T_Jutsu_SummonOwl", 128)`), `Tests/ShinobiSummonTests.cpp` (`JutsuHaveSealAnimationsAndIcons` includes `UGA_SummonOwl`)

- [ ] **Step 1:** Check the SpriteCook balance; tell the user the cost (low quality, about 2 credits) and wait for a yes. Generate in Misha's icon style, using the owl's front view (`owl_front.jpeg`) as reference: the plush owl, wings up, a blue lightning bolt.
- [ ] **Step 2:** Import with UE closed: `powershell -File Scripts/run_python.ps1 Scripts/setup_hud.py`.
- [ ] **Step 3: Run** `-Filter ShinobiCoop.Summons`. Expected: all pass (the icon loads).
- [ ] **Step 4: Commit** `feat: Summon Owl icon`.

### Task 7: Full check and docs

- [ ] **Step 1:** Full suite: `powershell -File Scripts/run_tests.ps1 -Filter ShinobiCoop`. Expected: all pass; report any failure verbatim.
- [ ] **Step 2:** Two-player PIE (listen server + client): on the client the owl follows smoothly and plays Strike; bolts land on enemies only.
- [ ] **Step 3:** Update `docs/vfx/sprite-vfx-guide.md` sections 10-11 (summons): the toad is retired, the owl's actor, numbers and sky bolt, and how she differs from the ground summons.
- [ ] **Step 4: Commit** `docs: the owl replaces the toad`. Hand the branch to the user for a playtest before any merge.
