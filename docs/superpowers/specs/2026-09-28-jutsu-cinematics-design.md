# ShinobiCoop: Jutsu Cinematics and Charge (Design)

**Date:** 2026-09-28. **Status:** approved in brainstorming, awaiting spec review.

## 1. Goal

Make the four jutsu look and play like the ones in Naruto games. Today each jutsu plays one generic Mixamo spell cast during a short wind-up and fires. After this work:

- **Fireball:** the caster makes hand seals, leans back to inhale, then leans forward and breathes a fireball out of their mouth.
- **Chidori:** after the seals, the caster holds lightning in their right hand, gripping the wrist with the left. The lightning crackles and grows, then the caster dashes in and strikes.
- **Rasengan:** after the seals, a spinning chakra ball forms and grows in the caster's right palm, shaped by the left hand. The caster then dashes in and slams it into the target.
- **Amaterasu:** the caster's eyes glow and bleed, then black flames ignite on the target and keep burning.
- **Hold to charge:** holding the jutsu button makes any jutsu stronger and visibly bigger, at a chakra cost.

**Success means:**
- each jutsu reads clearly from the third-person camera: seals, then the jutsu-specific body move, then the release;
- a tap still works and fires an uncharged jutsu after the seals;
- a held jutsu is visibly bigger and does more damage;
- co-op partners see the same phases and effects;
- the existing rules still hold: a staggering hit interrupts with a half refund, and enemies can still use the jutsu classes.

**Decisions made in brainstorming:**

| Topic | Decision |
|---|---|
| Timing | Cinematic: a short, always-played seal phase with the jutsu-specific movement, like the Naruto games. |
| Charge | Hold to charge, up to 2 s. Chakra drains while held. A full charge gives 1.75× damage and 1.8× visual size. |
| Rasengan | Becomes a dash-and-strike like Chidori, instead of an instant cone hit. |
| Animation source | Scripted in headless Blender 5.2: seal and body poses keyed on the Mixamo rig (including finger bones), exported as FBX, then fed through the existing retarget pipeline. No mocap and no paid packs. |
| VFX | Stays mesh and material only, with no Niagara (as in M2b). New Python-built flame and streak materials. |

## 2. Cast timeline and charge

Every jutsu runs in three phases. Each jutsu has one montage with three matching sections.

| Phase | Behaviour |
|---|---|
| **Seal** | Fixed length. Hand seals plus the jutsu's body move. Always plays in full, even on a tap. |
| **Charge** | Starts when the seals end, if the button is still held. A looping pose; the effect grows. Lasts until the button is released, the 2 s cap is reached, or chakra runs out. |
| **Release** | The jutsu fires and the cooldown starts. If the button was released during the seals, the release follows the seals directly. |

**Charge level.** `L = ChargeTime / MaxChargeTime`, from 0 to 1, with `MaxChargeTime = 2.0 s`.
- Damage multiplier: `1 + 0.75 · L`.
- Visual size multiplier: `1 + 0.8 · L`.
- Chakra drain: `ChargeDrainFraction = 0.5` of the base cost over a full charge, drained continuously on the server. When chakra reaches 0, the jutsu releases at the level reached.

**Per jutsu:**

| Jutsu | Seal (≈) | Charge pose | Release | What scales with L |
|---|---|---|---|---|
| Fireball | 0.6 s: seals at chest, ending in a lean back (inhale) | Held inhale, flame flickering at the mouth | Lean forward and blow; the ball spawns at the mouth | Damage, ball size, splash radius |
| Chidori | 0.5 s | Right hand low, left hand gripping the right wrist; lightning in the hand | Dash and strike (as today) | Damage, lightning size |
| Rasengan | 0.4 s | Right palm up, left hand hovering over it; the ball forms and grows | Dash (≈ 900 range, snaps to the nearest enemy in a 15° cone like Chidori), strike, knockback | Damage, ball size, knockback |
| Amaterasu | 0.4 s | Right hand near the face, staring; eyes glow and bleed | Black flames ignite on the target | Damage per tick, ignition burst size |

The seal lengths are starting points, tuned against the preview renders and screenshots.

**Interrupt.** A staggering hit (Event.Hit.Stagger) during Seal or Charge cancels the jutsu as today: half of the base cost is refunded and no cooldown starts. Chakra drained while charging isn't refunded.

**Enemies.** Enemy abilities built on the jutsu classes have no input to hold, so they always fire at `L = 0` right after the seals.

**Networking.**
- The owning client's input release reaches the server through GAS's replicated input events (`UAbilityTask_WaitInputRelease`, as `GA_ChakraCharge` and `GA_Sprint` already use). The server measures the charge time itself, so the server's `L` is authoritative.
- The montage section jump to Release (`MontageJumpToSection`) is replicated by the ASC.
- A new tag, `State.Casting.Charging`, is granted during the Charge phase. Remote machines grow the charge visuals from their own clock, starting when that replicated tag appears, capped at full size. No new replicated property is needed.
- Things created at release carry the server's `L`: the fireball projectile and `AShinobiBurst` get an initial-only replicated `Scale`.

## 3. Animation pipeline

**`Scripts/character/build_jutsu_anims.py`** (new, run with `blender -b -P`):
- Loads the `Power_Up_30fps.fbx` Mixamo rig, the same source skeleton the existing retarget pipeline uses.
- Holds a **pose library**: named poses, each a map from Mixamo bone to rotation, fingers included.
  - Seals: Tiger, Ram, Snake, Horse, Ox, Monkey, Rabbit.
  - Body poses: `lean_back_inhale`, `blow_forward`, `chidori_hold`, `rasengan_hold`, `eye_focus`, `dash_strike`.
- Builds one action per jutsu as a keyed sequence of poses with timings and easing, split into Seal, Charge and Release. The Charge part loops seamlessly: its first and last keys match, with a small breathing and tremble motion in between.
- May layer a downloaded Mixamo clip underneath as base body motion (for example `Praying` for hands-together, or `Standing 2H Cast Spell 01`), but only where the previews show it helps.
- Writes `SourceArt/Mixamo/Jutsu/<Jutsu>.fbx`, plus a preview strip of key frames (front and side views) under `Saved/JutsuPreviews/` for review.
- Writes the section boundaries to `SourceArt/Mixamo/Jutsu/phases.json`, which is the single source for the phase timings.

**`Scripts/setup_jutsu_anims.py`** (updated):
- Imports these FBX files instead of the generic Mixamo casts.
- Retargets them as today (`RTG_MixamoToManny`, whose finger chains are already exact-mapped; `RTG_MannyToHero` carries them on to the hero).
- Builds the montages with three sections from `phases.json`: `Seal` links to `Charge`, and `Charge` links to itself so it loops.

**Ability side:** the montage plays from `Seal` and flows into the looping `Charge`. On release, the ability jumps to `Release`. The jutsu's effect (projectile, dash, ignition) starts at the Release section's start plus a per-jutsu offset (for example, Fireball's blow lands at +0.1 s).

**Iteration loop:** poses are tuned from the Blender preview renders first, since that is cheap. The user reviews those strips before the engine import. Then the in-engine `-ShinobiJutsuShot` screenshots are checked for each phase, on the hero mesh.

**Risk:** the poses are authored as numbers, not by hand, so expect a few render-and-tweak rounds per seal.

## 4. VFX

**New materials in `Scripts/setup_vfx.py`:**

| Material | Look | Used for |
|---|---|---|
| `M_Flame` | Unlit, additive. World-space noise panning upward, eroded by an `Erode` parameter so it shows flame-tongue edges and flickers. Params: Color, Intensity, Erode, Speed. | Fire breath, fireball shell, mouth flame |
| `M_FlameDark` | The same shape logic, but translucent, since black can't be additive: a near-black core with a purple-red rim. | Amaterasu flames |
| `M_Streak` | Unlit, opaque, dark red | Eye-blood streaks |

**`UShinobiCastFxComponent` becomes phase-aware.** It reads `State.Casting.<Jutsu>` and `State.Casting.Charging`, and sizes the effects by its local charge timer.

| Jutsu | Effects |
|---|---|
| Fireball | No hand orb. During Charge: a small flickering `M_Flame` at the mouth (head bone plus offset) with an orange light, growing with the charge. At Release: a flame cone (a stretched cone mesh) bursts from the mouth for about 0.3 s. Projectile: the existing glow core plus an `M_Flame` shell, scaled by `1 + 0.8 · L`. |
| Chidori | The hand orb plus 6 lightning arcs. Each arc is a short chain of thin, jagged segments, re-randomised about every 50 ms, and grows longer with the charge. A hard light flicker. The arcs stay on through the dash. |
| Rasengan | A bright core plus 2 counter-rotating shells with a panning swirl. Grows from 0.25 m to 0.45 m across with the charge. Stays on through the dash. The existing `Spiral` burst on hit, scaled. |
| Amaterasu | On the caster: two red eye-glow dots, and blood streaks under the eyes that draw downward during the seals. Both attach to the head bone with per-mesh eye offsets (hero and Manny), tuned from screenshots. On the target: a new black-flame cluster of 10 flame tongues (`M_FlameDark`) around the body, flickering, shown while the replicated `State.Burning.Black` tag is on. It replaces the purple body aura for that state; the aura stays as a faint rim. The cluster is a fixed size, because GAS replicates tags to every client but not effect magnitudes. The charge shows instead as a scaled ignition burst at release. |

**`AShinobiBurst`** gets an initial-only replicated `Scale`, so impacts match the charge. New kind: `BlackFlameIgnite`.

## 5. Components and files

| Unit | Change |
|---|---|
| `UShinobiJutsuAbility` | Replaces the single `WindUp` with Seal, Charge and Release. Adds `MaxChargeTime`, `ChargeDrainFraction`, `GetChargeLevel()`, a server-side drain tick and auto-release, the montage section jump, and grants `State.Casting.Charging`. `ExecuteJutsu()` is called at release, with the level readable by subclasses. |
| `GA_Fireball` | Launches from the mouth. Scales damage, size and splash radius. |
| `GA_Chidori` | Scales damage. Otherwise as today. |
| `GA_Rasengan` | Rewritten as a dash and strike, sharing Chidori's dash approach. Scales damage and knockback. |
| `GA_Amaterasu` | Scales damage per tick. Spawns the ignition burst. |
| `AShinobiProjectile`, `AShinobiBurst` | Initial-only `Scale`. |
| `UShinobiCastFxComponent` | Phase-aware effects per jutsu, as in §4. |
| Body aura / new black-flame component | Flame-tongue cluster while `State.Burning.Black` is on. |
| `ShinobiTags` | `State.Casting.Charging`. |
| `Scripts/character/build_jutsu_anims.py` | New Blender script (§3). |
| `Scripts/setup_jutsu_anims.py`, `Scripts/setup_vfx.py` | Updated (§3, §4). |

## 6. Testing

**Automation tests** (extend `ShinobiJutsuTests.cpp`):
- the charge-level math;
- the drain amount over a full charge;
- auto-release at the cap and when chakra reaches 0;
- a tap fires at `L = 0` after the seals;
- an interrupt during Charge refunds half the base cost, starts no cooldown and doesn't refund the drain;
- enemies fire uncharged;
- the projectile and burst `Scale` replicate;
- a Rasengan dash hits and knocks back;
- the cast FX shows the right parts for each phase.

**Visual checks:**
- the Blender preview strips, reviewed by the user before the engine import;
- `-ShinobiJutsuShot` screenshots for each phase of each jutsu;
- the smoke test.

## 7. Out of scope

- Niagara particle systems (revisit in M5 if the mesh look falls short).
- Facial animation (cheeks puffing, mouth opening). The Mixamo hero has no face rig, so the fire comes from a mouth offset on the head bone.
- Camera cinematics (zoom-ins or slow motion on cast).
- Charge for melee and the other abilities.
