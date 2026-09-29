# ShinobiCoop M2b (Jutsu Visuals): Lean Plan

**Goal:** make M2a's jutsu and statuses visible on every machine. The effects are mesh and material only, built on the existing `M_ChakraBodyAura`, with no Niagara.

**Process:** at the user's request (2026-09-26), this is built inline, without per-task subagent reviews. There is one light review at the end, and the looks are checked with the `-ShinobiJutsuShot` screenshot run. Branch `feature/m2b-jutsu-visuals`, cut from `feature/m2a-jutsu-mechanics` (M2a's manual check is still pending).

## Design

| Effect | How |
|---|---|
| Burn, Black Flame, Shock on the body | `UShinobiChakraAuraComponent` becomes the body aura for any state. It moves to `AShinobiCharacterBase`, so dummies and enemies show it too. It shows the highest-priority state: black flame (dark purple), then burn (orange), then shock (white-blue, hard random flicker), then chakra charge (blue). It still uses one overlay applied at `BeginPlay`, and only Fade, Color and Intensity change. |
| Hand orb during a jutsu | New `UShinobiCastFxComponent` on the base character: a glowing sphere plus a light on `hand_r`. It follows the new `State.Casting.<Jutsu>` tags, which each jutsu grants through `ActivationOwnedTags` (replicated to everyone). The looks: fire for Fireball, a crackling white-blue orb for Chidori, a bigger pulsing blue orb for Rasengan, dark purple for Amaterasu. |
| Fireball projectile | The grey ball becomes a glowing fire sphere (`MakeGlow`, Core 1) with an orange point light. |
| Impacts, Firestorm, Substitution log | New `AShinobiBurst`: a server-spawned, replicated one-shot actor. Its initial-only `Kind` selects an expanding, fading glow sphere with a light. Kinds: Fire (fireball impact), Firestorm (a 1000-unit sphere, matching the 500 radius), Lightning (Chidori hit), Spiral (Rasengan hit), and Substitution (a grey smoke puff plus a brown log that stays for 2.5 s at the old position). |
| Shared look | `ShinobiVfx::MakeGlow` returns a dynamic instance of `M_ChakraBodyAura` with Color, Intensity, Core and Inflate set. `setup_vfx.py` adds a `Core` parameter to the material (0 = rim only, as before; 1 = glowing all over). |

**Deviation from the spec:** the spec says "All VFX are spawned through Gameplay Cues". Here, persistent effects follow replicated tags and one-shots are replicated actors. Native C++ cue notifies would still need Blueprint assets and cue-manager asset paths, while this approach is two classes and one component. Revisit in M5 if cue batching or pooling becomes worth it.

**Part 2, done 2026-09-26 (was deferred for the downloads):** hand-seal and cast wind-up animations (`Cast`, `Fireball`, `Chidori`, `Rasengan`, `Amaterasu` FBX files in `SourceArt/Mixamo/`), retargeted with the `setup_chakra_pose.py` pipeline, then played during `WindUp`.

## Steps
1. Tags `State.Casting.*`, and the jutsu grant them. ✔
2. `ShinobiVfx::MakeGlow`, plus the `Core` material parameter in `setup_vfx.py` (re-run it with the editor closed). ✔
3. The body aura generalised and moved to the base character. ✔
4. `UShinobiCastFxComponent`. ✔
5. `AShinobiBurst`, spawned from the projectile, Chidori, Rasengan (hit + Firestorm) and Substitution. ✔
6. Fireball projectile look. ✔
7. Test `ShinobiCoop.Jutsu.VfxFollowCastingStatusAndImpact`, plus the existing aura test, which must still pass.
8. Build, run the tests, `setup_vfx.py`, `shot_chakra.ps1 -Jutsu`, look at every shot and tune. Then the smoke test.
9. One light review, then the user's look check (together with M2a's 16-row check).

## Part 2: cast animations (done)

Mixamo has no Naruto hand seals or jutsu-specific moves, so each jutsu uses one generic spell cast from the user's downloads, all in the `JUTSU_CLIPS` table in `Scripts/setup_jutsu_anims.py`: Fireball = `Fireball`, Chidori = `Standing 1H Magic Attack 01`, Rasengan = `Standing 2H Magic Attack 01`, Amaterasu = `Standing 1H Magic Attack 03`.

- **Pipeline.** Each clip is imported animation-only onto the Power Up Mixamo skeleton, retargeted with `RTG_MixamoToManny`, root-locked with no root motion, and wrapped in `AM_<Jutsu>`.
- **Playback.** `UShinobiJutsuAbility::CastMontage` plays from activation. It is sped up (capped at 1–2.5×) so that `CastReleaseFraction` (0.45) of the clip lands at the end of the wind-up. The follow-through keeps playing after the jutsu fires; a staggering interrupt stops it.
- **Checked:** by the `-ShinobiJutsuShot` screenshots. 42/42 tests and the smoke test pass.

**Superseded (2026-09-29):** the jutsu now use Blender-scripted seal/charge/release clips, see `2026-09-28-jutsu-cinematics.md`.

**Real hand seals (optional later):** video-to-mocap with finger tracking (e.g. DeepMotion Animate 3D, Rokoko Vision or Move.ai) from a hand-seal reference video, retargeted the same way. Or key 3–5 seal poses on Manny's finger bones in Blender. Or a paid ninja/hand-seal pack from Fab.
