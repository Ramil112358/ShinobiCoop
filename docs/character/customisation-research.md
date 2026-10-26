# Character customisation: how games do it, and how to build it on MetaHumans

Research notes (2026-10-24) for the in-game character creator: body and face customisation on MetaHuman bodies that every
outfit still fits. Background: the plan (phases 0-5) and the phase 0 spike results are in memory note
`character-creator`; outfit fitting is in [outfit-fitting-research.md](outfit-fitting-research.md).

## What games do

Few studios publish their pipelines. The table combines official docs, interviews, modder write-ups and what the
creators expose. Treat the "how" column as informed reconstruction unless a source is cited.

| Game | Face | Body | Outfits on different bodies |
|---|---|---|---|
| Black Desert | Presets + ~35 "pull points" moved freely (XYZ position, rotation, size) | Region sliders, but the build is locked to the class | Clothes follow the bone-driven body |
| Throne and Liberty | "Designer" presets, Easy Face Maker, sliders from brow depth to lip shape | Height, shoulder, chest, forearm, calf … | Follow the sliders |
| AION 2 (UE5) | 200+ settings, X/Y graphs for eyes and face contour | Separate rib cage, chest, upper chest and lat sliders | Follow the sliders |
| EA Sports FC (Clubs) | Scanned/authored head presets ("Cranium" heads) + style presets | Height/weight grid (also changes stats) | Kits made for a few body types |
| Baldur's Gate 3 | Preset faces + "attachments" (hair, scars, tattoos), no sliders | A few fixed body types | Armour made per body type, so no clipping even in cutscenes |
| The Sims 4 | Presets + sculpt sliders (morphs) | One body mesh + morph sliders | Every garment has a second UV map pointing into "deformer maps": body morphs are applied to all clothing in the same way |
| Starfield | Presets + sliders | A blend of three extremes (thin, muscular, heavy) picked on a grid | — |
| Dragon's Dogma 2 | Presets + sliders | Muscle and size sliders | Armour warps to the body; heavy armour exaggerates size |

The common pattern:

1. **Presets carry quality.** Every game starts from authored heads. Sliders tweak a preset; they don't build a face.
2. **Face sliders are joints (bones) in MMOs.** Pull points and region sliders move or scale face joints; the skin
   follows its weights. Morph-based sculpting (Sims) is the other school, and it costs a morph per asset.
3. **Bodies are a few base types plus continuous sliders.** The base types are male/female, sometimes frames. Within a
   type, sliders are mostly bone scales, sometimes with one to three blend morphs (Starfield's thin, muscular, heavy).
4. **Outfits are authored per base body type, then follow the sliders automatically.** Bones carry through for free.
   Morphs need a transfer: Sims-style deformer maps, or copies with matching morphs.
5. **Players never see the body under clothes.** Covered skin is hidden (masks or deleted faces).
6. **There are many save slots, and presets can be shared.** AION 2 keeps eight appearance slots and eight download
   slots per character.

## What MetaHuman gives us in UE 5.8 (checked in engine source)

| Feature | Editor | In game | Notes |
|---|---|---|---|
| MetaHuman Creator: face/body solve, texture synthesis, autorig | yes | **no** | The face/body solver is in `MetaHumanCoreTechLib`, an Editor module; autorig and textures need Epic's cloud |
| Creator scripting: `TranslateFaceLandmarks`, `Get/SetFaceModelCoefficients`, body measurements | yes | no | Lets us author face presets and slider extremes from scripts |
| RigLogic (`FAnimNode_RigLogic`) | yes | **yes** | Writes every face joint as **DNA neutral + delta** (`AnimNode_RigLogic.cpp:633`), so face-joint edits must run **after** it |
| Per-component post-process anim (`SetOverridePostProcessAnimBP`) | yes | **yes** | Our C++ instance can run RigLogic and then apply slider offsets, per character |
| DNA editing (DNACalib modules) | yes | yes (Runtime modules) | But `USkelMeshDNAUtils::UpdateJoints/UpdateBaseMesh/RebuildRenderData` are editor-only, so a new face mesh can't be baked at runtime |
| Face morph targets (RigLogic blendshapes) | — | yes | LOD0 detail only, while face joints exist at every LOD |
| Chaos Outfit Asset (fits an outfit to any MetaHuman body) | yes | **no** (`WITH_EDITOR`) | Fit each outfit to our base bodies in the editor and ship those copies |
| Body morph targets on Chaos cloth | — | **no** | Chaos only blends one cloth-asset morph (for resizing); pinned cloth follows skinning (spike, 2026-10-24) |
| Bone scales after retarget | — | **yes** | Carries into leader-pose outfits, heads and cloth (spike); `UShinobiBodyComponent::SetBoneShape` |
| Mutable | yes | yes | Invalidates a MetaHuman face's DNA once the mesh changes; graphs can't be built from scripts. Not used |
| Grooms (hair, brows, beard) | yes | yes | Bound to a face mesh; follow its skinning. Large face-joint offsets near brows and hairline need a check |

## Recommended design for Shinobi

### Face: presets → blend → feature sliders → colours

1. **Head presets** (FC/BG3 quality):
   - **MetaHuman heads:** RamilMH, conversions, and about 8-12 new faces from the Creator. These get everything below.
   - **Static grafted heads:** Ramil (Tripo), Hero, Bika, Cat, Misha, Pabu (`graft_head.py`). These get colours and
     hair only, with no expressions or sliders.
2. **Blend between MetaHuman presets** (the Creator's own model, done at runtime):
   - All MetaHuman faces share topology and joint names. For each preset, store offline its **joint deltas** from the
     base face's DNA neutral, and its LOD0/1 **vertex deltas** as morph targets on the base face.
   - At runtime, mix them by weight: joint deltas in our post-RigLogic node, morphs with `SetMorphTarget`.
   - Joints carry the shape at every LOD, and eyes and teeth follow because they hang off the same joints. Morphs add
     LOD0 detail on top.
3. **Feature sliders** (MMO pull points): about 30-60 sliders. Each is a list of face joints with a translation,
   rotation and scale at −1 and +1:
   - jaw width and length, chin, cheekbones;
   - nose bridge, width and tip;
   - eye position, size and tilt; brow height; mouth width; ears.
   - The extremes come from Creator variants: move landmarks with `TranslateFaceLandmarks`, then read the neutral
     joint differences from the DNA.
   - Applied after RigLogic, so expressions keep working on the reshaped face.
   - Symmetric by default, with an asymmetry toggle like AION 2's X/Y graphs.
4. **Colours and layers:**
   - skin tone as material parameters on the preset's baked skin (texture synthesis is editor-only, so tint, don't
     regenerate);
   - eye colour, makeup and tattoo decals;
   - groom swaps with melanin and redness.

### Body: base frames → bone-scale sliders → optional detail morphs

1. **Base bodies:** a male and a female MetaHuman body, optionally 2-3 frames each (slim, average, heavy) built in the
   Creator. Each outfit is fitted once per base, as BG3, FC and Fortnite do.
2. **Continuous sliders as bone scales** (verified in the spike): height, shoulders, chest, arms, forearms, waist,
   hips, thighs, calves. Limbs scale through MetaHuman twist/corrective helper bones (leaves). The torso scales through
   chain bones with the inverse on their children. Ranges stay clamped to about 0.85-1.25.
3. **Optional detail morphs** (belly, muscle definition, chest): body-only and small enough to stay inside clothes,
   or carried by the outfit copies' skinned (not cloth) parts via `transfer_morphs.py`. This is our version of the Sims
   deformer maps.
4. **Height:** body scale plus capsule height. Cloth collision capsules grow with the bone shapes (or are built for
   the largest size).

### Outfits

- **One fitted copy per base body:** today's `build_outfits.ps1`, keeping MetaHuman helper weights, with hide masks
  and Chaos cloth. Bone-scale sliders then carry every copy for free.
- **Fab MetaHuman outfits:** fit each one to every base body with the Chaos Outfit Asset in the editor, export, and
  run the same pipeline (cloth, hide mask).
- **Heads, hats and hair:** hair/hat interplay via groom swaps or a "hat hair" variant, as most MMOs do.

### Data, network, saving

- **`FShinobiAppearance`**, about 100 bytes: base body, head preset, up to 3 blend presets with weights, face slider
  bytes, body slider bytes, colours, groom ids and outfit id. Replicated on the pawn; the server clamps every field;
  each client rebuilds locally with no mesh baking.
- **Saves:** a local SaveGame with several slots, plus copy/paste preset strings (AION 2's download slots, without a
  backend).
- **UI:** tabs for Preset, Face, Body, Skin, Hair, Outfit; camera focus per tab; randomise, undo, symmetry.

## Risks to prove first

1. **Face-joint sliders on a MetaHuman face look good**, with expressions intact. Check at LOD0 and LOD3, and with
   brows, lashes and beard grooms.
2. **Preset blending:** joint and morph deltas from two Creator faces mix without breaking eyelids or lips.
3. **Cloth collision** on the widest bone-shape settings.
4. **Packaged build:** the RigLogic post-process override and morphs survive cooking.

## Sources

- Aion 2 creator ([allthings.how](https://allthings.how/aion-2-character-customization-guide-every-face-hair-and-body-slider/), [Dot Esports](https://dotesports.com/mmo/news/aion-2-chest-size-slider), [ExitLag](https://www.exitlag.com/blog/aion-2-character-creation/))
- Throne and Liberty creator ([Dexerto](https://www.dexerto.com/gaming/how-to-create-a-character-in-throne-and-liberty-2917576), [mein-mmo](https://mein-mmo.de/throne-and-liberty-charakter-editor/))
- Black Desert pull points ([Steam discussion](https://steamcommunity.com/app/582660/discussions/0/2132869574258633874), [PC Gamer sliders](https://pcgamer.com/pc-game-sliders))
- Baldur's Gate 3 presets ([Kotaku](https://kotaku.com/baldurs-gate-3-character-creator-races-genitals-options-1850653066))
- EA Sports FC height/weight and Cranium heads ([magicgameworld](https://www.magicgameworld.com/ea-fc-26-how-to-change-your-pros-height-and-weight-to-perfect-your-build/), [soccergaming FC editor](https://soccergaming.com/forums/threads/fce-character-editor.6476177/))
- Sims 4 deformer maps ([Sims 4 Studio](https://sims4studio.com/thread/10806/), [MorphMaker](https://simscommunity.info/2018/07/10/learning-your-tools-the-sims-4-morphmaker/2))
- Starfield body grid ([Sportskeeda](https://www.sportskeeda.com/esports/starfield-complete-character-customization-guide))
- Dragon's Dogma 2 armour fitting ([LaptopMag](https://laptopmag.com/gaming/i-hate-dragons-dogma-2s-character-creator-and-its-not-for-the-reason-you-think), [Steam](https://steamcommunity.com/app/2054970/discussions/0/4289188948006846727))
- Bone scaling vs morphs ([GameDev.net](https://gamedev.net/forums/topic/549203-how-character-customization-work/), [Epic forums](https://forums.unrealengine.com/t/customizing-character-with-morph-targets/748892), [cloth + morphs](https://dilly.gitbook.io/dilly/cloth-physics-+-morph-targets))
- MetaHuman outfits ([Tailoring wardrobe items](https://dev.epicgames.com/documentation/en-us/metahuman/tailoring-your-own-wardrobe-items), [runtime resizing not possible (Epic forum)](https://forums.unrealengine.com/t/tutorial-chaos-cloth-outfit-asset-resizing-addendum/2647376))
- MetaHuman DNA and RigLogic ([DNA Calibration](https://github.com/EpicGames/MetaHuman-DNA-Calibration), [RigLogic whitepaper](https://cdn2.unrealengine.com/rig-logic-whitepaper-v2-5c9f23f7e210.pdf), [MetaHuman 5.8 release notes](https://dev.epicgames.com/documentation/metahuman/metahuman-5-8-release-notes-in-unreal-engine), [Mutable breaks MetaHuman faces](https://forums.unrealengine.com/t/mutable-destroys-metahuman-face-advanced-rig-logic-for-mutable/2354600))
- Engine source, UE 5.8: `Plugins/Animation/RigLogic` (`AnimNode_RigLogic.cpp`, `SkelMeshDNAUtils.h`), `Plugins/ChaosOutfitAsset` (`OutfitAsset.cpp`), `Plugins/ChaosCloth` (`ChaosClothingSimulationMesh.cpp`), `SkeletalMeshComponent.h` (`SetOverridePostProcessAnimBP`)
