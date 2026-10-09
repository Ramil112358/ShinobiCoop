# Surface Traversal: Walls and Water

Revised 2026-10-09. Replaces the 2026-10-05 version; the custom movement mode it described is dropped.

## Goal

Let player-controlled shinobi run up and along building walls and walk on water, in the style of *Naruto to Boruto: Shinobi Striker*. While they traverse a wall or water, a thin blue chakra glow wraps each foot, in the same look as the chakra-charge aura (hold C). Players can fight while on a wall. Collision is never bypassed, so no character can enter a building through its walls. The feature must work in co-op with client prediction.

## Agreed decisions

- **Engine custom gravity.** On a wall, the character's gravity direction is set to point into the wall (`UCharacterMovementComponent::SetGravityDirection`), and the character uses ordinary `MOVE_Walking` on it. The engine already handles capsule rotation, floor finding, prediction, client corrections and replication to simulated proxies (`ACharacter::ReplicatedGravityDirection`). There is no custom movement mode.
- **Automatic entry.** No new button. Walking, sprinting or falling into a climbable face while movement input pushes into it attaches the character.
- **Jump kicks off.** Jump on a wall resets gravity and launches the character away from the wall and slightly up.
- **Top edge vaults.** Climbing past the top of a wall puts the character upright on the roof.
- **Full wall combat.** Melee, jutsu, dash and dodge can all be used on a wall.
- **Players only.** Enemies, summons and other AI keep their existing movement.

## Movement rules

### Entry
- The owner must be an `AShinobiPlayerCharacter`. It must not be mounted, downed, dead or phased.
- The character is in `MOVE_Walking` or `MOVE_Falling`.
- The movement input points into a blocking `WorldStatic` hit whose normal is within 25° of horizontal (`|Normal.Z| <= sin 25°`).
- The hit actor is not tagged `ShinobiNoSurfaceTraversal`.
- An overlap test of the capsule, rotated to the wall frame and placed at feet-contact distance, finds no blocking geometry. If it does, the character does not attach. This is what stops the capsule rotating into a building.
- Standing beside a wall with no input into it never attaches.

### On the wall
- Gravity direction is `-WallNormal`. The capsule up axis is `WallNormal`.
- The camera stays world-upright and never rolls. Movement input maps from camera space onto the wall plane. The part of the input pointing into the wall moves the character up, and the part pointing out of it moves the character down. The part along the wall moves the character sideways. With the camera facing the wall, W is up, S is down and A/D are sideways. There is no away-from-wall input exit, because S already means "down". Sprint and chill speed rules apply unchanged.
- Faces less than about 1.8 m tall never attach. A head-height trace must also reach the face, so kerbs, benches and car sides are excluded.
- At a corner, when the character reaches an adjacent face that also passes the entry test, gravity re-targets to the new face. It changes only after a sweep confirms the move.

### Exit
Every exit restores gravity to `(0, 0, -1)` and an upright yaw-only rotation.

| Trigger | Result |
| --- | --- |
| Jump | Launch along `WallNormal * KickOffSpeed + Up * KickOffUp`; the character enters falling |
| Moving down the wall onto walkable ground | Stands up on the ground |
| Floor is lost at a side edge (outer corner) with no adjacent face to wrap onto | Falling |
| Floor is lost at the top edge while moving up | Trace down onto the roof from above the edge; if the trace finds a walkable spot, place the character there standing. Otherwise, falling |
| Mount, downed, dead, knockback launch, Teleport, Substitution | Immediate detach |

### Water
- Water surfaces get the `ShinobiWaterSurface` tag. Their mesh collision is query-only, blocks `Pawn` and ignores every other channel. Characters walk on them as ordinary floor, with no special movement code. Materials, transforms and shadows stay unchanged.
- London's hidden river-edge barriers (`City:X`) are removed, so players can step onto the water. The valley has no river-edge barriers. Its map-wide `RiverWater` plane lies under the terrain everywhere except the river bed, so blocking `Pawn` on it affects only the river.
- Water blocks every `Pawn`, so enemies and summons also walk on the surface instead of wading on the river bed. Navmesh then covers the river.
- Outer map-boundary walls (valley `EdgeWall*`, London `EdgeWall`) keep blocking and are tagged `ShinobiNoSurfaceTraversal`.

### Building collision
- Climbable building geometry must block `Pawn`. Imported London building chunks (`City:B`) are checked. If they lack collision, the build script gives them collision so players cannot walk into buildings.

## Combat on walls

- Five call sites force upright yaw-only rotation: `GA_Melee`, `ShinobiDashJutsu`, `ShinobiJutsuAbility` (cast facing), `GA_Substitution`, `GA_Teleport`. One helper, the static `UShinobiMovementComponent::FaceDirection(AActor*, const FVector& WorldDir)`, replaces the facing in `GA_Melee`, `ShinobiDashJutsu` and `ShinobiJutsuAbility`. Substitution and Teleport detach the character before relocating it, so their upright rotation stays correct. It builds a rotation whose up axis is `-GravityDirection` and whose forward axis is `WorldDir` projected onto the gravity floor plane. For an upright character it gives exactly the old yaw-only result.
- Projectiles and beams keep using the camera aim and need no change.
- Teleport and Substitution relocate the character, so they detach it first.
- When a character on a wall receives `LaunchCharacter` knockback (melee push, Rasengan, Almighty Push), it detaches before the launch is applied.
- Dragon fire patches already require an upward floor normal and spawn nothing on a wall. Mounting detaches.

## Animation

`ABP_Manny_Combat` is checked first. If its ground speed comes from world-XY velocity, it is changed to gravity-relative velocity (velocity projected onto the plane perpendicular to gravity). Wall running then plays the normal locomotion cycle in the rotated frame.

## Foot chakra VFX

- `UShinobiSurfaceFxComponent` is a new player-only scene component.
- It attaches one thin, flattened glow shell to the `foot_l` and `foot_r` bones of the body mesh. The material comes from `ShinobiVfx::MakeGlow` with `ShinobiVfx::Colors::Chakra`, the same look as the charge aura. A subtle pulse and a faint light sit at the feet.
- The glow shows while the character is on a wall, or on a floor whose actor is tagged `ShinobiWaterSurface`.
- It fades in and out over about 0.15 s. Death, downed and mount hide it at once.
- It is purely cosmetic and not replicated. Each machine derives the state from replicated gravity direction and movement base. Nothing is created on a dedicated server, and it has no chakra cost.

## Out of scope

AI wall traversal, ceilings, swimming, water ripples or splashes, traversal stamina or cost, and a dedicated traversal button.

## Implementation areas

- `ShinobiMovementComponent.h/.cpp`: entry detection, gravity switching, the wall input frame, exits, top-edge vault, corner re-target, and the queries `IsWallTraversing()` / `IsWaterTraversing()`.
- `ShinobiPlayerCharacter.cpp`: map `Move` input to the wall plane, kick off on Jump, detach on mount, death and knockback.
- `UShinobiMovementComponent::FaceDirection` replaces the upright-only facing in four ability call sites. Teleport and Substitution detach first.
- New `Shinobi/VFX/ShinobiSurfaceFxComponent.h/.cpp`.
- `ABP_Manny_Combat`: gravity-relative ground speed, if needed.
- `Scripts/world/build_world.py` and `Scripts/london/build_london.py`: water tag and collision, edge-barrier response, boundary tag, building collision check.
- New `Tests/ShinobiSurfaceTraversalTests.cpp`.

## Testing

- **Focused automation tests** `ShinobiCoop.Movement.SurfaceTraversal.*`:
  - run-in entry
  - no entry while only standing beside a wall
  - entry blocked when the rotated capsule does not fit
  - tangent climbing
  - Jump kick-off
  - top-edge vault
  - standing on a water-tagged surface
  - `FaceDirection` keeps the up axis on a wall and gives yaw-only rotation when upright

  Only these tests run. The full suite is not run.
- **In-editor co-op check** at the end, with two players on both maps:
  - wall entry and exits
  - combat on a wall
  - walking on water and reaching it from the bank
  - no clipping into buildings
  - the foot VFX visible to the other player
  - mount precedence
  - no persistent client correction

## Acceptance criteria

1. A player runs into a building wall and runs up and sideways on it. Jump kicks them off, and climbing past the top lands them on the roof.
2. No player can enter a building through its walls, whether by entry, climbing, corners or kick-off.
3. A player walks from the bank onto the river and across it on both maps without falling to the river bed.
4. Melee, jutsu, dash and dodge work on a wall without snapping the character upright.
5. Blue chakra wraps both feet during wall and water traversal. The other co-op player sees it, and it disappears after detaching.
6. Ground movement, sprint, mounted flight and AI movement are unchanged.
