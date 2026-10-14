# Block with Susanoo ribcage armour

Date: 2026-10-14. Status: approved in chat, awaiting spec review.

## Goal

Players can hold a guard. While guarding, a close-fitting spectral Susanoo ribcage (the Susanoo jutsu's blue-violet M_Susanoo glow, 1.75 m tall so it stays below the head, 1 m across)
wraps the character, as in Naruto to Boruto: Shinobi Striker's Sasuke guard. Blocking cuts incoming damage, costs
stamina per hit, and breaks when stamina runs out.

Out of scope: blocking for PvP bots and enemies, parry/perfect-block timing, a HUD element (the stamina bar already shows
the cost).

## Gameplay: `UGA_Block`

- Input: hold `F` (keyboard) or `Gamepad_LeftTrigger`, new `EShinobiAbilityInputID::Block` mapped in
  `ShinobiInputConfig.cpp`. Granted to players only.
- Hold pattern copied from `UGA_Sprint`: `CanActivateAbility` requires stamina > 0; `WaitInputRelease` ends it.
- Asset tag `Ability.Block`. Activation blocked by `State.Dodging`, `State.ChakraCharging`, `State.Phased`,
  `State.Mounted` (plus the shared stun/stagger/downed/dead/PvP-hold blockers from `UShinobiGameplayAbility`).
- On activation: cancels `Ability.Melee` and `Ability.Sprint`. While active it blocks `Ability.Melee`,
  `Ability.Jutsu`, `Ability.Sprint`, `Ability.ChakraCharge`. Dodge stays usable (dodging ends the block).
- On authority it applies infinite `UGE_Blocking`, granting `State.Blocking` and `State.StaminaRegenBlocked`. A GE
  (not activation-owned tags) so the tag replicates to every machine for the visual. On end it applies
  `UGE_StaminaRegenDelay` before removing `UGE_Blocking` (same ordering as sprint).
- The ability also ends when `State.Blocking` is removed from outside (guard break), via a tag-removed wait.
- Movement: guarding roots the player (decided after the spec review: the player AnimBP only has the full-body
  `DefaultSlot`). `UShinobiMovementComponent::GetMaxSpeed` returns 0 while `State.Blocking`; the player still turns to
  face their input. Dodge cancels the block (`Ability.Block` added to `UGA_Dodge::CancelAbilitiesWithTag`); a jutsu
  being cast blocks it (`Ability.Block` added to `UShinobiJutsuAbility::BlockAbilitiesWithTag`).

### Damage (`UShinobiDamageExec`)

Constants on `UGE_Blocking`: `DamageTakenScale = 0.2`, `StaminaPerDamage = 0.5`, `MinStaminaCost = 5`.

When the target has `State.Blocking`, after the existing Susanoo/attack-boost scaling and before the shield:

- Periodic damage (burning): scaled by `DamageTakenScale`, no stamina cost.
- Direct hit: cost = `max(MinStaminaCost, StaminaPerDamage * Damage)` using the damage before the block scale.
  - Stamina >= cost: stamina base -= cost; damage *= `DamageTakenScale`; no `Event.Hit.Stagger`, no physical
    reaction shove; the exec spawns the spark burst (`AShinobiBurst`, `SusanooRise` at scale 0.15) at the victim.
  - Stamina < cost (guard break): stamina base set to 0; remove effects granting `State.Blocking`; the hit lands
    unscaled; applies `UGE_Stagger` and sends `Event.Hit.Stagger` once, regardless of the 20-damage threshold.
- `State.Invulnerable` / `State.Phased` checks keep running first, unchanged.

New tags: `Ability.Block`, `State.Blocking`.

## Visual

- Generalise `UShinobiSusanooComponent`: tag, mesh path, fade-in/out seconds and attach offset become properties
  (defaults stay the current Susanoo values). Players get a second instance configured for `State.Blocking`,
  `SM_SusanooRibs`, 0.15 s fade-in, 0.25 s fade-out, same `M_Susanoo` glow material.
- Spark burst: the existing `EShinobiBurst::SusanooRise` (violet), server-spawned and replicated, at scale 0.15 at the
  victim on each blocked hit (`UShinobiDamageExec`). Raising the guard has no burst (its star card read as a star over the head).

## Asset: `SM_SusanooRibs`

1. Concept image: "spectral purple Susanoo ribcage armour, hollow, empty, wide gaps between ribs, curved ribs around an
   invisible person, isolated on plain background, 3/4 view".
2. Rodin Gen-2 image-to-3D (Tripo fallback).
3. Blender cleanup: delete interior/inner shell so a character fits and shows through, decimate to about 20k tris,
   origin at the feet, +X forward, centimetres, one material slot. Export `SourceArt/Props/susanoo_ribs.fbx`.
4. `Scripts/setup_susanoo_ribs.py` imports it to `/Game/Shinobi/Jutsu/SM_SusanooRibs` (modelled on
   `setup_susanoo.py`), no collision.

Fallback if the generated shell can't be opened up cleanly: a procedural rib mesh in Blender, extending
`Scripts/character/make_susanoo.py`.

## Pose

Mixamo "Standing Block Idle" (downloaded by the user to `SourceArt/Mixamo/Standing Block Idle.fbx`), retargeted to
Manny by `Scripts/setup_jutsu_anims.py` as `AM_Block_Guard`. Tag-driven like the mounted sit loop: while
`State.Blocking` the player keeps it looping in the full-body `DefaultSlot` whenever no other montage plays, on every
machine, and stops it when the tag goes.

## Testing (targeted only)

Automation tests in `Source/ShinobiCoop/Tests/`:

- Blocking target takes 20% of a direct hit and pays the stamina cost.
- Blocked hit sends no stagger event.
- Hit costing more stamina than left breaks the guard: full damage, `State.Blocking` removed, stagger.
- Burning while blocking: 20%, no stamina cost.
- Input: `F` and `Gamepad_LeftTrigger` map to the Block action.
- Visual: the block component shows `SM_SusanooRibs` once `State.Blocking` is granted.

## Dependencies and risks

- Imports run through `Scripts/run_python.ps1` (editor commandlet, GUI editor closed); `unreal-mcp` is not needed.
- Rodin credits: one generation, more if the shell comes out closed.
- Image-to-3D models usually come back as closed solids; opening up the ribs is the main risk (fallback above).
