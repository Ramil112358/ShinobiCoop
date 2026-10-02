# ShinobiCoop: Jutsu Pages and Movement/Defense Jutsu (Design)

**Date:** 2026-10-02. **Status:** approved in brainstorming, awaiting spec review.

## 1. Goal

The player has four jutsu (Fireball, Chidori, Rasengan, Amaterasu) on keys 1-4. More are wanted: teleportation, Kamui, Almighty Push, earth and water walls, then meteors, a water dragon, Susanoo and summons. This is the first of several cycles. It delivers:

- a **page system** so keys 1-4 can reach more than four jutsu;
- five jutsu: **Teleport (Flying Raijin)**, **Kamui**, **Almighty Push**, **Earth Wall**, **Water Wall**.

**Success means:**
- Page switching works in combat, is shown on the HUD, and a key fires the ability of the current page only.
- Each jutsu uses the existing seal/charge/release flow, chakra cost, cooldown and interrupt rules.
- Co-op partners see the same effects (kunai, phasing, shockwave, walls).
- Enemies react: they cannot hit a phased player, are pushed by the shockwave, and are blocked by walls.

**Later cycles (own spec each):** B elemental AOE (fire meteor, ice meteor shower, water dragon); C Susanoo power-up; D summons (toad, snake, dogs); E rideable dragon.

**Page map (all cycles):**

| Page | Slots 1-4 |
|---|---|
| 1 | Fireball, Chidori, Rasengan, Amaterasu (existing) |
| 2 | Teleport, Kamui, Almighty Push, Earth Wall |
| 3 | Water Wall, Water Dragon, Fire Meteor, Ice Meteor |
| 4 | Susanoo, Summon Toad, Summon Snake, Summon Dogs |
| 5 | Summon Dragon |

This cycle fills page 2 and puts Water Wall on page 3, slot 1.

## 2. Page system

- A page is a loose gameplay tag `State.JutsuPage.1` ... `State.JutsuPage.5` on the player's ASC. The owning client adds it locally (so `CanActivate` predicts correctly) and tells the server through a server RPC on the player controller; the server adds it on the ASC. Exactly one page tag is present; page 1 at spawn.
- Keys stay bound to `Jutsu1..Jutsu4`. Each jutsu ability gets a `Page` property (default 1) and sets `ActivationRequiredTags` to its page tag in the constructor. Several abilities can share one `InputID`; only the one on the active page can activate.
- Granting: all jutsu abilities are granted at spawn (as the four are today).
- Switching: **Q** or mouse wheel cycles forward (wheel up/down cycles both ways); gamepad **RB + D-pad left/right**. Ignored while any jutsu ability is active (`Ability.Jutsu` tag present).
- HUD (`ShinobiHUDWidget`): the four slots read the ability class for `(current page, slot)`; empty slots are hidden. A row of page pips sits under the slots. The slot cooldown, affordability and flash logic is unchanged.
- Page count is a constant (`NumJutsuPages = 5`); pages with no abilities yet are skipped when cycling.

## 3. Jutsu

All inherit `UShinobiJutsuAbility`. Numbers are starting values, tuned in playtest.

### 3.1 Teleport (Flying Raijin) - `GA_Teleport`
- Chakra 25, cooldown 10 s, seal 0.4 s.
- Release: three marker kunai are thrown in a fan (±12° yaw) toward the aim point. Each is a replicated actor, `AShinobiMarkerKunai`, flying at 4000 uu/s, sticking where it hits (surface or max range 2500), and living 6 s.
- The ability stays active after the throw (it does not call `EndJutsu` until done). The base class gains a hook so a jutsu may hold itself open after release; the cooldown still starts at release.
- Pressing the jutsu key again within 6 s teleports to the stuck kunai nearest the crosshair (angle from the aim ray; oldest kunai if none is stuck near it). On arrival: the kunai is consumed, a burst plays, enemies within 200 uu take 20 damage and a stagger. The other kunai stay for further teleports until the 6 s run out; the last teleport or the timeout ends the ability and removes any left.
- The server does the move (`TeleportTo`, the way `GA_Substitution` does it); owning clients get the corrected position. The re-press uses `UAbilityTask_WaitInputPress`.
- Destination is validated server-side: a kunai must belong to the caster and be stuck. No teleport into geometry (the kunai's surface normal offsets the arrival point by capsule radius).
- Interrupt: a staggering hit during the seal cancels as usual. After release the player may be hit freely; nothing cancels the open window except death or downing.

### 3.2 Kamui - `GA_Kamui`
- Chakra 30, cooldown 12 s, seal 0.3 s, duration 3 s.
- New tag `State.Phased` granted by a gameplay effect for the duration.
- While phased:
  - `UShinobiDamageExec` ignores direct hits like `State.Invulnerable` but does **not** send `Event.Substitution`.
  - The capsule ignores `ECC_Pawn` and projectile channels (walking through enemies; projectiles pass through), restored on end.
  - Enemy AI targeting skips phased characters (`ShinobiAIController` target check) and loses its current target.
  - The player cannot melee or cast jutsu (`ActivationBlockedTags` on those abilities); dodge, sprint and movement still work.
- The player can end it early by pressing the key again.
- Visual: replicated tag drives a body fade (material parameter on the body mesh; outline stencil off) to ~5% for the owner's enemies, ~25% for teammates, plus a swirl effect on the body (SpriteCook spiral sheet) at start and end.
- Ending while inside an enemy or wall capsule: the server nudges the character to the nearest free spot (depenetration, standard `FindTeleportSpot`).

### 3.3 Almighty Push (Shinra Tensei) - `GA_AlmightyPush`
- Chakra 30, cooldown 8 s, seal 0.5 s.
- Release delay hits the key moment of the release clip. The server finds characters of the opposing team within 600 uu × `GetVisualScale()`, launches them away from the caster (horizontal 1500 uu/s plus 400 vertical, `LaunchCharacter`), applies 25 × `GetDamageScale()` damage (so a staggering hit) and `State.Staggered` briefly.
- Does not push the caster or allies.
- Visual: expanding shockwave ring mesh on the ground at the caster, dust burst (SpriteCook sheet), screen shake via the existing feedback component.
- Charge (existing base mechanism) scales radius and force.

### 3.4 Earth Wall and Water Wall - `GA_EarthWall`, `GA_WaterWall`, `AShinobiBarrier`
- Both spawn `AShinobiBarrier`, a replicated actor, 450 uu wide, 300 uu tall, 60 uu thick, centred 350 uu in front of the caster at ground level, facing the caster's facing.
- The actor has an HP pool and a lifetime. It blocks pawns and projectiles (collision), takes damage from projectiles and melee hits that reach it (through the normal damage path; the actor owns a small ASC-less hit interface, see 3.5), and breaks at 0 HP or when the lifetime ends.
- Rise animation: the actor interpolates from below the ground over 0.4 s (replicated start time; local interpolation). Break: crumble burst and removal.

| | Earth Wall | Water Wall |
|---|---|---|
| Chakra | 35 | 25 |
| Cooldown | 14 s | 9 s |
| Seal | 0.5 s | 0.4 s |
| HP | 400 | 250 |
| Lifetime | 10 s | 6 s |
| Look | Poly Haven rock mesh slab, PBR rock material | Water material from the river surface, translucent slab, ripple scroll |

- One wall per caster at a time: casting a new wall breaks the previous one of that caster.
- Allies and the caster collide with the wall like anyone else (simple, predictable). Friendly projectiles and the caster's own jutsu pass through it (the barrier ignores projectiles whose instigator is on the players' team).

### 3.5 Hit interface for barriers
`AShinobiProjectile` and enemy melee currently damage via the ASC. `AShinobiBarrier` has no ASC; add a tiny `IShinobiDamageable` interface (`ReceiveShinobiDamage(float, AActor* Instigator)`) that projectile impact and the melee hit helper call when the hit actor implements it. `AShinobiBarrier` implements it. No other actor needs it now.

## 4. Assets

- **Kunai:** look for a free three-prong kunai on Fab or Sketchfab (licence recorded in `docs/world/asset-credits.md`); generate with Hyper3D Rodin only if no usable one is found.
- **Earth wall:** existing Poly Haven rock in the world library (`Content/Shinobi/World/Library`); a flattened, scaled slab with a rock PBR material.
- **Water wall:** the existing river surface material as a base, as a translucent instance.
- **Effects:** mesh and material effects only (no Niagara), per the existing rule. SpriteCook sheets via the sheet-on-black atlas method for the Kamui swirl and push dust (cost noted in `docs/vfx/sprite-vfx-guide.md`).
- **Animation:** reuse the existing seal and body poses from the Blender pose library; new poses (a throw for Teleport, a palm push for the shockwave, ground-slam for walls) added to `build_jutsu_anims.py`.
- **HUD icons:** painted icons for the five jutsu via SpriteCook, imported with the existing HUD script. The slot shows the display name until the icon exists.

## 5. Components and files

| Unit | Change |
|---|---|
| `ShinobiTags` | `State.JutsuPage.1-5`, `State.Phased`, `Cooldown.Jutsu.Teleport/Kamui/AlmightyPush/EarthWall/WaterWall`, `State.Casting.*` for the new jutsu where a hand effect is needed |
| `UShinobiJutsuAbility` | `Page` property; page tag in `ActivationRequiredTags`; hook for holding open after release |
| `ShinobiPlayerController`, `ShinobiPlayerCharacter` | Page state, switch input (Q, wheel, pad), server RPC, block while casting |
| `ShinobiInputConfig` | Page-next/previous actions |
| `ShinobiHUDWidget` | Slots from the current page; page pips |
| `GA_Teleport`, `AShinobiMarkerKunai` | New |
| `GA_Kamui`, `UShinobiDamageExec`, `ShinobiAIController`, body fade | New and changed |
| `GA_AlmightyPush` | New |
| `GA_EarthWall`, `GA_WaterWall`, `AShinobiBarrier`, `IShinobiDamageable` | New; projectile and melee call the interface |
| `ShinobiEffects` | `GE_Kamui` (granted tag + duration) |
| `Scripts/` | Anim poses, icon import, kunai/wall asset setup |
| `Tests/ShinobiJutsuTests.cpp` (or a new file) | Tests in section 6 |

## 6. Testing

Automation tests:
- Page gating: with page 2 active, key 1 fires Teleport and not Fireball; switching is refused while a jutsu is active; page wraps and skips empty pages.
- Teleport: three kunai spawn; a second press moves the caster to the selected kunai; the window expires after 6 s and removes the kunai; a foreign kunai is rejected.
- Kamui: damage is ignored while phased, `Event.Substitution` is not sent, the tag clears after 3 s, offensive abilities are blocked, collision restores after.
- Push: only opposing characters in radius are launched; allies and the caster are not.
- Barrier: blocks a projectile, loses HP from it, breaks at 0 HP, expires after its lifetime; a second wall replaces the first.
- Damage exec: `State.Phased` target takes no damage from instant or periodic effects.

Visual checks: `-ShinobiJutsuShot` screenshots for each new jutsu, HUD page screenshots, and a two-client co-op run for replication.

## 7. Out of scope

- Water wall dousing burns; walls that move or can be pushed.
- Teleporting other players or enemies; teleport to a marked player.
- Kamui letting the player attack from the phase; ranged pass-through for enemies.
- Niagara, camera cinematics, new characters.
- Cycles B-E.

## 8. Risks

- **Holding an ability open after release** changes a base-class contract (every jutsu currently ends in `ExecuteJutsu`). Mitigation: the hook is opt-in; existing jutsu are untouched, and their tests keep passing.
- **Kamui collision changes** can leave the player stuck in geometry. Mitigation: depenetration on end, and a test.
- **Replicated page tag** can desync between client prediction and the server. Mitigation: the client request is authoritative on the server; a rejected switch (mid-cast) restores the client tag from the server's replicated state.
