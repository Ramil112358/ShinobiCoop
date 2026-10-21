# Outfit fitting: how games fit clothes to different bodies

Research notes (2026-10-20) for making the cloak outfits (Hokage, Akatsuki) and future skins fit every roster character. Background: [outfit-steps.md](outfit-steps.md) describes the current pipeline, which skins one rigid cloak over every body.

## The problem in the current cloaks

Seen in play (2026-10-19 screenshots, Hokage and Akatsuki on the Hero):

- **The skirt stretches when jumping.** The hem is skinned partly to the thigh bones, so a raised knee drags it up into long stretched triangles.
- **The underarms tear.** `skin_outfit.py` splits the sleeve off the coat below the shoulder so the sleeve can follow the arm. With the arm raised, the split shows as gaps and dark holes.
- **Arms come out through the Akatsuki sleeves.** The script turns each sleeve about the shoulder onto the arm and lifts the cloth off the body, but the forearm still ends up outside the sleeve wall.

These are limits of the approach itself, not one bad mesh: one cloak, weights copied from one body, no simulation.

## What games do

Throne and Liberty, Battlefield and Fortnite don't publish their outfit pipelines. The sources below describe the common approach.

1. **Outfits are made per body type, not stretched over every body.** Fortnite skins are built for a few set body types. A fan port of a Throne and Liberty outfit is labelled "for body type 2", which suggests the game ties outfits to body types too.
2. **The body under the clothes is hidden.** Body polygons the garment covers are masked out or deleted, so skin and shirt can't poke through. MetaHuman outfits ship a body mask for this ("Body Hidden Face Map"), and Character Creator has "Delete Hidden Faces". One Reallusion user found hiding fixed only part of their clipping; the rest came from cloth not colliding with the body.
3. **Loose cloth is simulated.** Skirts, wide sleeves and capes run cloth physics and collide with the body's physics capsules. They swing and trail instead of being dragged by the leg and arm bones.
4. **Close-fitting parts are skinned carefully.** Weights are transferred from the body and cleaned by hand. Matching topology between body and garment helps, plus extra clearance in problem areas and corrective shapes at shoulders and armpits.
5. **Clearance tools.** RadFit (Blender) pushes clothing off the skin with a set gap and colours the garment by distance to the body. NeverClip (Unity) pushes geometry out of intersections in the shader.

## What UE 5.8 already ships

All found under `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins`:

- **Chaos Cloth** (`ChaosCloth`, `ChaosClothAsset`, `ChaosClothAssetEditor`, Dataflow nodes). Simulates loose cloth on a skeletal mesh, colliding with the physics asset.
- **Chaos Outfit Asset** (`ChaosOutfitAsset`). Parametric, resizable clothing: one outfit is fitted to different body sizes. Each size names a source garment and source body parts; the asset finds the closest size to a body from its mesh or measurements, and an interpolation point count trades resize quality against cost. Built around MetaHuman bodies. In 5.6, resizable clothing didn't support custom skinning, Control Rig or RBAN. A user on Epic's forum found that keeping the cloth simulation through a resize isn't solved: baking the resize corrupted the sim-to-render deformation, and Epic suggested making two outfit assets.
- **Mutable** (`Mutable`, plus experimental `MutableClothing`, `MutableDataflow`). Runtime character customisation: merges parts into one mesh. A Mutable mesh uses the reference mesh's physics asset by default, which can fail when parts differ a lot in proportion; "Physics Asset Merging" in the compile options fixes that.

## Target look (decided 2026-10-20)

- **Idle:** the cloak sits neatly on the body. No visual artifacts: no body parts or arms showing where they should be hidden inside the cloak.
- **Moving (walking, running, jumping):** the hem and sleeves swing and trail behind.

## Sources

- [Chaos Outfit Asset resize thread (Epic forums)](https://forums.unrealengine.com/t/intended-workflow-for-resizing-a-chaos-outfit-asset-while-preserving-cloth-simulation/2762838)
- [FChaosSizedOutfitSource API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/ChaosOutfitAssetEngine/FChaosSizedOutfitSource)
- [Parametric clothing: essential terminology](https://dev.epicgames.com/documentation/unreal-engine/getting-started-with-parametric-clothing)
- [Mutable physics and clothing](https://dev.epicgames.com/documentation/unreal-engine/mutable-physics-and-clothing-in-unreal-engine)
- [Tactical Clothes Base 01 (body mask)](https://superhivemarket.com/products/tactical-clothes-base-01-)
- [Reallusion: CC4 to Unreal "Delete Hidden Faces"](https://discussions.reallusion.com/t/cc4-unreal-delete-hidden-faces/12712)
- [Throne and Liberty outfit port, "for body type 2" (Patreon)](https://www.patreon.com/posts/134843717)
- [Throne and Liberty costume slots (Sportskeeda)](https://www.sportskeeda.com/mmo/does-throne-liberty-transmog-system)
- [Fortnite modular skins (Sportskeeda)](https://www.sportskeeda.com/fortnite/fortnite-why-snap-one-worst-customizable-skins-time)
- [Polycount: clothing vs body clipping](https://polycount.com/discussion/comment/1748964/)
- [Blender Artists: skin showing through clothing](https://blenderartists.org/t/stop-characters-skin-showing-through-clothing-best-practice-for-unity-export/1109386)
- [Unity: skinned cloth tips](https://discussions.unity.com/t/skinned-cloth-tutorials-tips-and-tricks/500364)
- [RadFit](https://superhivemarket.com/products/radfit--fit-any-outfit-to-any-body)
- [NeverClip](https://discussions.unity.com/t/released-neverclip-removes-common-clipping-bugs/554979)
