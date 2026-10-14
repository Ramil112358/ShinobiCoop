# Block with Susanoo ribcage armour

Date: 2026-10-14. Status: approved in chat, awaiting spec review.

## Goal

Players can hold a guard. While guarding, a close-fitting spectral Susanoo ribcage (purple, glowing, about 2.2 m tall)
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
- Movement: `UShinobiMovementComponent::GetMaxSpeed` scales by `UGE_Blocking::SpeedScale = 0.4` while
  `State.Blocking`.

### Damage (`UShinobiDamageExec`)

Constants on `UGE_Blocking`: `DamageTakenScale = 0.2`, `StaminaPerDamage = 0.5`, `MinStaminaCost = 5`.

When the target has `State.Blocking`, after the existing Susanoo/attack-boost scaling and before the shield:

- Periodic damage (burning): scaled by `DamageTakenScale`, no stamina cost.
- Direct hit: cost = `max(MinStaminaCost, StaminaPerDamage * Damage)` using the damage before the block scale.
  - Stamina >= cost: stamina base -= cost; damage *= `DamageTakenScale`; no `Event.Hit.Stagger`, no physical
    reaction shove; send `Event.Block.Hit` (cue for the spark burst).
  - Stamina < cost (guard break): stamina base set to 0; remove effects granting `State.Blocking`; the hit lands
    unscaled; send `Event.Hit.Stagger` regardless of threshold (applies `UGE_Stagger`).
- `State.Invulnerable` / `State.Phased` checks keep running first, unchanged.

New tags: `Ability.Block`, `State.Blocking`, `Event.Block.Hit`.

## Visual

- Generalise `UShinobiSusanooComponent`: tag, mesh path, fade-in/out seconds and attach offset become properties
  (defaults stay the current Susanoo values). Players get a second instance configured for `State.Blocking`,
  `SM_SusanooRibs`, 0.15 s fade-in, 0.25 s fade-out, same `M_Susanoo` glow material.
- Spark burst: existing `ShinobiBurst` sprite VFX in purple, spawned locally when the armour rises and on each
  `Event.Block.Hit` at the hit side of the ribcage.

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

Mixamo "Standing Block Idle" (downloaded by the user), retargeted to Manny, played as a looping upper-body montage while
`GA_Block` is active and stopped on end. The montage slot must exist in the player AnimBP; if not, add an upper-body
slot blend in `SandboxCharacter_CMC_ABP`.

## Testing (targeted only)

Automation tests in `Source/ShinobiCoop/Tests/`:

- Blocking target takes 20% of a direct hit and pays the stamina cost.
- Blocked hit sends no stagger event.
- Hit costing more stamina than left breaks the guard: full damage, `State.Blocking` removed, stagger.
- Burning while blocking: 20%, no stamina cost.
- Input: `F` and `Gamepad_LeftTrigger` map to the Block action.
- Visual: the block component shows `SM_SusanooRibs` once `State.Blocking` is granted.

## Dependencies and risks

- `unreal-mcp` must be connected for the import, montage and AnimBP work.
- Rodin credits: one generation, more if the shell comes out closed.
- Image-to-3D models usually come back as closed solids; opening up the ribs is the main risk (fallback above).
