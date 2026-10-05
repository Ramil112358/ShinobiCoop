# Surface Traversal: Walls and Water

## Goal

Let player-controlled shinobi automatically climb solid vertical walls and walk across marked water surfaces by moving into them. Show blue chakra around the character's feet only while this traversal is active. Keep the feature compatible with co-op movement prediction and the existing world maps.

## Agreed design

- Traversal engages automatically from movement input; it does not require a new button.
- Use a dedicated surface-traversal movement mode in `UShinobiMovementComponent`. Do not reuse flying mode, which is also used by dragon mounts and could bypass normal collision behavior.
- Limit automatic traversal to player characters. Enemies and other AI keep their existing movement.
- Detect climbable surfaces from solid world collision and near-vertical hit normals. Project movement along the contacted face, keep the character attached while input continues, and detach when the surface ends or the player moves away.
- Mark water surface actors explicitly with `ShinobiWaterSurface`. Water surfaces must block the character capsule for traversal while retaining their existing visual material and map layout.
- Drive the foot chakra effect from the active traversal state on each machine; keep it cosmetic and use the existing chakra-blue VFX look.
- Preserve sprint, mounted flight, jutsu movement effects, and ordinary walking/falling behavior when traversal is inactive.

## Surface authoring and maps

The Hidden Leaf Valley map creates a river plane tagged `ShinobiWater` with `NoCollision`; London imports water mesh chunks as `City:W`, also with `NoCollision`. Both currently let players fall to or walk on the river bed. The implementation will add the `ShinobiWaterSurface` marker and query collision to these surfaces so the movement component can identify and stand on them. The water meshes remain visually unchanged. Existing invisible water-edge walls remain collision barriers; they are not water surfaces.

Climb detection uses blocking world geometry with an approximately vertical face, rather than requiring content authors to retag every building mesh. It is restricted to player-controlled characters and movement-directed contact, so merely standing beside a wall does not pull a character onto it. Surface changes at corners and edges must be swept and validated before the character follows them.

## Movement and networking

`UShinobiMovementComponent` owns detection, entry/exit, surface-relative input, adhesion, and movement-mode transitions. The regular character movement collision sweep remains authoritative. Client prediction and saved moves must preserve traversal intent/state sufficiently for the server and owning client to agree without persistent correction. Replicated movement state lets remote players see the correct pose and foot effect.

On a wall, movement is constrained to the contacted plane; input toward the wall maintains adhesion, while input away from it releases the character to falling. On water, the character remains supported at the marked water plane and moves horizontally. Leaving the marked plane returns to ordinary falling or walking according to the next valid floor hit. Mounted flight takes precedence and never enters surface traversal.

## Chakra effect

Add a small cosmetic component or equivalent effect owned by the player character. It follows the feet and turns on while the movement component reports wall or water traversal. It uses the project's existing chakra-blue glow style, is visible to nearby co-op players, and has no gameplay or chakra-cost effect. It turns off promptly on detach, death, or mount entry.

## Scope and exclusions

- Player-controlled shinobi only; AI traversal, ceilings, swimming, underwater movement, traversal stamina/cost, and a dedicated traversal input are out of scope.
- Water detection is explicit through the `ShinobiWaterSurface` marker, not inferred from material names.
- Building geometry does not need per-mesh authoring tags; any solid near-vertical world face may be climbed by a player.
- No changes to water appearance, map dimensions, or existing river-edge barriers.

## Implementation areas

- `ShinobiMovementComponent.h/.cpp`: surface-traversal state, prediction, collision queries, and custom movement handling.
- `ShinobiPlayerCharacter.h/.cpp` or a focused traversal VFX component: expose player eligibility and show the foot chakra effect.
- `Scripts/world/build_world.py` and `Scripts/london/build_london.py`: mark the generated water surfaces and enable the collision response needed for character support.
- In-editor checks for wall entry, water support, edge detach, and mounted flight will validate the result, including a co-op session.

## Acceptance criteria

1. A player can move onto a vertical building face, climb up and sideways, and detach by moving away or reaching an unsupported edge.
2. A player can move from a bank onto marked water and walk over the water surface without falling to the river bed.
3. Blue chakra appears around the feet during both traversal types, is visible to the other co-op player, and disappears after detaching.
4. Ordinary ground movement, sprint, jutsu movement, and mounted flight continue to behave as before.
5. Generated Hidden Leaf Valley and London maps mark water surfaces for traversal while their water-edge barriers continue to block movement.
