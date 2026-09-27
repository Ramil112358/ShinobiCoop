# ShinobiCoop Sub-project #2: Hero Character from Photos (Design)

**Date:** 2026-09-27. **Status:** approved in brainstorming, awaiting spec review.

## 1. Goal

Replace the grey Manny player model with a hero character based on the user: an anime-style likeness (face shape, hair, build, clothing colours) drawn as a Naruto-style ninja, cel-shaded like the rest of the game.

**Success means:**
- in game, every player is the hero;
- every existing animation (locomotion, combo, dodge, chakra charge, the four jutsu casts, downed pose) plays correctly on it;
- the chakra, burn and shock auras, the cast orb and the death ragdoll work on it;
- co-op partners see it too.

**Decisions made in brainstorming:**

| Topic | Decision |
|---|---|
| Scope | One hero (the user). All co-op players use it; teammates are told apart by a floating name tag, not a tint. Character select and per-player models are out of scope. |
| Style | Anime likeness: recognisably the user, in a shinobi outfit (headband etc.). Not a photoreal scan. |
| Generator | Higgsfield (the connector in this Claude session) for the A-pose reference and image-to-3D. The user consented to uploading their photos there. |
| Rigging | Mixamo auto-rig (approach A). The user uploads the prepared mesh and places the markers (~2 min). Mixamo's weighting tolerates AI-generated meshes and gives finger bones (useful for real hand seals later). |
| Animation | Runtime retarget. The Manny mesh stays as a hidden driver running every existing animation; the visible hero mesh copies its pose each frame. No animation is re-exported or duplicated. |

## 2. Pipeline

| # | Stage | Who / tool | Output (under `SourceArt/Character/`) | Checkpoint |
|---|---|---|---|---|
| 0 | Photos | The user drops 3 photos | `Photos/` | — |
| 1 | Anime A-pose front reference | Claude, Higgsfield image generation with the photos as reference: full body, front, arms ~45° down, hands open (not in pockets), neutral face, anime ninja outfit, plain light background. ~4 candidates per round. | `Reference/front_*.png` | The user picks one or asks for another round |
| 2 | Back view | Higgsfield, from the chosen front: same character and outfit from behind | `Reference/back.png` | Quick check |
| 3 | Image → 3D | Higgsfield `generate_3d` on the chosen reference (plus the back view if the model accepts several views). 2–3 tries. | `Mesh/raw_*.glb` | Claude renders turnarounds in Blender; the user picks one |
| 4 | Blender prep | `Scripts/character/prep_mesh.py`, run headless (`blender -b -P`) | `Mesh/hero_prep.fbx`, `Mesh/preview_*.png` | Renders checked |
| 5 | Rig | The user on mixamo.com, following `docs/character/mixamo-steps.md` | `Mesh/hero_rigged.fbx` | — |
| 6 | UE import | `Scripts/setup_hero.py` (UE Python commandlet) | `/Game/Shinobi/Hero/...` | Screenshots |

**Stage 4 (Blender prep) steps, in order:**
1. Import the GLB and apply transforms.
2. Scale to the user's height (180 cm unless they say otherwise), feet at the origin, facing front (-Y).
3. Weld vertices and delete loose parts smaller than 1% of the mesh.
4. Decimate to ≤ 60,000 triangles.
5. Keep a single material; bake to one texture if the GLB has several.
6. Export FBX with an embedded texture.
7. Render front and side previews.

The script stops with a clear message if a check fails: no mesh, triangles still over budget after decimation, or height out of range.

**Stage 6 (UE import) creates:**
- the `SK_Hero` skeletal mesh with its own skeleton;
- `PA_Hero`, the physics asset for the ragdoll;
- `MI_Hero`: the texture into base colour, Substrate, plain lit (the existing `M_ToonPost` supplies bands and outlines);
- `IK_Hero`, an IK rig with chains matching `IK_Manny`;
- `RTG_MannyToHero`, the retargeter;
- a check that every retarget chain maps.

**Privacy.** Photos, references, raw GLBs and `hero_prep.fbx` stay local and are gitignored. Only `Mesh/hero_rigged.fbx` (the source of the committed asset) and the imported `/Game/Shinobi/Hero` assets are committed. Uploads happen only in stages 1–3 (Higgsfield) and stage 5 (the mesh to Mixamo, done by the user).

## 3. In-game integration

**Classes:**
- **`UShinobiRetargetAnimInstance`** (C++): a native AnimInstance with a custom proxy that runs `FAnimNode_RetargetPoseFromMesh` (IKRig plugin), the pattern the engine's own `IKRetargetAnimInstance` uses. The source is the attached parent mesh, and the asset is `RTG_MannyToHero`. No Animation Blueprint asset is needed.
- **`AShinobiPlayerCharacter`:**
  - `HeroMesh` is a `USkeletalMeshComponent` attached to the Manny mesh (same relative transform), using `SK_Hero` and the retarget anim class.
  - The Manny mesh becomes the hidden driver: not visible, no shadow, and `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones` so it keeps animating.
  - Abilities, montages and replication are unchanged.
- **`AShinobiCharacterBase::GetBodyMesh()`** returns the visible mesh (the hero when present, else `GetMesh()`). It's used by:
  - the chakra aura overlay (`UShinobiChakraAuraComponent`);
  - the cast orb attachment (`UShinobiCastFxComponent`: a hand-bone name per body);
  - the death ragdoll (`OnDeadTagChanged`: simulate the body mesh with `PA_Hero`).
  Hit-stop keeps freezing the driver, and the hero follows.
- **Name tags:** each other player gets a floating name (their player name) above their head, reusing `UShinobiOverheadTextComponent` in a name mode. The local player sees no tag on themselves.
- **Fallback:** if `SK_Hero` or `RTG_MannyToHero` is missing, or the cvar `Shinobi.Hero` is 0 (default 1), the Manny mesh stays visible and no `HeroMesh` is created, which is today's behaviour.
- **Scope:** enemies and training dummies stay (tinted) Mannys.

## 4. Error handling

| Situation | Handling |
|---|---|
| Reference off-likeness, wrong pose, hands hidden, busy background | Regenerate with a tightened prompt; nothing proceeds without the user's pick |
| 3D result poor (melted hands, fused arms, mushy face) | Retry with the back view or a cleaner reference. After 3 bad tries, stop and present options (another service, a different reference) |
| Blender prep check fails | The script exits non-zero with the reason; the preview renders show the problem |
| Mixamo rejects or mis-rigs the mesh | The markers/symmetry fix is in `mixamo-steps.md`; re-run prep with adjustments (e.g. wider arm gap) if needed |
| Retarget artefacts (foot sliding, bent wrists, shoulders) | Tune `IK_Hero` chains and the retarget pose in `setup_hero.py`; check by screenshots |
| Assets missing at runtime | Fallback to the visible Manny (§3) |

## 5. Testing

- **Automation tests:**
  - with the assets present, the player has a `HeroMesh` using `UShinobiRetargetAnimInstance`, and the driver mesh is hidden but ticking pose;
  - with `Shinobi.Hero 0`, there's no `HeroMesh` and Manny is visible;
  - `GetBodyMesh()` returns the hero, and the aura overlay and dead ragdoll act on it.
- **Screenshots:** the `-ShinobiShot`, `-Jutsu` and `-Look` harnesses show the hero idle, charging chakra, casting each jutsu (orb in the right hand), fighting, downed and dead.
- **Regression:** existing tests and both smoke tests stay green.
- **Manual:** a 2-player playtest checklist in the implementation plan.

## 6. Milestones (lean process)

| # | Milestone | Done when |
|---|---|---|
| C1 | Reference | The user has picked an anime A-pose front view (+ back view) |
| C2 | Mesh | A 3D mesh is picked and prepped; preview renders look right |
| C3 | Rig | `hero_rigged.fbx` is back from Mixamo |
| C4 | In game | Hero playable with all animations, auras, cast orb, ragdoll, name tags; tests and screenshots pass; user playtest |

C1–C3 involve the user at each checkpoint. The C4 code (anim instance, `GetBodyMesh`, fallback, name tags) can be built and tested against a stand-in rigged mesh (the Mixamo `Power_Up_30fps` mannequin already in `/Game/Shinobi/Mixamo`) while C1–C3 are under way.
