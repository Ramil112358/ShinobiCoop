# Rigging the generated characters on Mixamo

Enemies and villagers wear characters generated with Hyper3D Rodin. Each one is prepped by `Scripts/character/prep_mesh.py`, which scales it to 180 cm and writes front and side preview renders. The characters are assigned in code:

| Who | Characters | Where in code |
|---|---|---|
| Rogue Ninja (melee) | Haru, Kuro | `AShinobiRogueNinja` `BodyPool` |
| Kunai Thrower (ranged) | Akane, Ryo | `AShinobiKunaiThrower` `BodyPool` |
| Rogue Jonin (boss) | Tetsuo | `AShinobiRogueJonin` `BodyPool` |
| Villagers: townsfolk | Goro (farmer), Hana (merchant), Mika | `AShinobiVillager::Bodies` |
| Villagers: guards | Jin, Suzu | `AShinobiVillager::Bodies` |

Each spawned enemy wears a random character from its type's pool. Each villager wears one character, chosen by its `Variant`; the valley has 6 routes and 5 villager characters, so the 6th repeats Goro.

Two more are planned but not made yet: Kaito (a third Rogue Ninja) and Sen (an elder). Rodin's text-only output for them either couldn't be rigged (arms fused to the body, a staff fused to the hand) or ignored the brief. Make them from a front-view concept image instead, using Rodin's image reference. No character appears on both sides; `ShinobiCoop.Cast.NoFaceOnTwoSides` checks this. The player roster (`AShinobiPlayerCharacter::Roster`) is separate; see "Player characters from photos" below. Until a character is rigged and imported, its NPC stays the tinted mannequin.

Each character's files live in `SourceArt/Character/Models/<Name>/`: `<Name>.glb` (the Rodin output), `<Name>_prep.fbx` and `preview_front.png`.

## For each character (about 2 minutes)

1. On mixamo.com, click **Upload Character** and drop in `<Name>_prep.fbx`.
2. Place the markers and pick the skeleton exactly as in [mixamo-steps.md](mixamo-steps.md): chin, wrists, elbows, knees and groin, with **Standard Skeleton (65)**. If a character holds something (Kuro's dagger, Sen's staff), put the wrist marker on the wrist itself, not on the object.
3. Download **FBX Binary, With Skin**, with no animation picked, and save it as `SourceArt/Character/Models/<Name>/<Name>_rigged.fbx`.

When some or all are saved, close the Unreal Editor and run:

```
Scripts/run_python.ps1 -Script Scripts/setup_hero.py
```

Each `<Name>_rigged.fbx` becomes `/Game/Shinobi/Characters/<Name>/`. Then `Scripts/run_tests.ps1 -Filter ShinobiCoop.Cast` checks for each imported character that it is worn, hides its mannequin and stands full size.

Only `<Name>_rigged.fbx` and its extracted `<Name>_rigged.png` are committed (`.gitignore`).

## Adding or swapping a character

Generate a model (a full-body, symmetric A-pose with empty hands works best), then:
1. Save it as `SourceArt/Character/Models/<Name>/<Name>.glb`.
2. Run `blender -b -P Scripts/character/prep_mesh.py -- <Name>.glb <Name>_prep.fbx` in that folder.
3. Rig it as above.
4. Add `<Name>` to an enemy `BodyPool` or to `AShinobiVillager::Bodies`, and rebuild.

## Player characters from photos (the roster)

The roster characters (Hero, Bika, Cat, Misha, Pabu) are made from real photos, but not straight from them: Hyper3D Rodin copies the pose of a photo (a seated cat stays a seated cat, an arm behind the back stays fused to the body), and Mixamo can't rig that. They go through an A-pose reference first, like the original hero:

1. **Reference images.** In one ChatGPT chat per character, attach the photos and ask for the character as a full-body anime design in a strict symmetric A-pose (arms about 45° down, open hands, legs apart with a clear gap, nothing held), wearing the same plain dark shinobi outfit (black top, dark trousers, shin bandages, blue sandals, no cloak: outfits are worn over it, see [outfit-steps.md](outfit-steps.md)). An animal becomes an upright humanoid with the animal's head, fur and colours. Then ask for the side and back views of "this exact character, same outfit, same scale, same pose". Save `<Name>_base_front.png`, `_side.png`, `_back.png` in `SourceArt/Character/Reference/` (not committed). If the side sheet shows two profiles, use one.
2. **3D model.** Upload the three views to Rodin in that order (Gen-2.5 Medium, Raw, 50k polygons, GLB) with a prompt like "one 3D character from three views of the same character: front, side, back, anime cel-shaded, strict symmetric A-pose, plain dark shinobi outfit, no cloak, no props". Save it as `SourceArt/Character/Models/<Name>/<Name>.glb`.
3. **Prep, rig, import.** `Scripts/character/prep_mesh.py` and the Mixamo steps are the same as for the NPC cast above. `SourceArt/Character/Models/Hero/Hero_rigged.fbx` is the hero itself (`/Game/Shinobi/Hero`, used before any other roster character); every other `<Name>_rigged.fbx` imports to `/Game/Shinobi/Characters/<Name>/`.
4. **Roster.** Add a line to `AShinobiPlayerCharacter::Roster` (`{TEXT("<Name>"), TEXT("/Game/Shinobi/Characters/<Name>/SK_<Name>.SK_<Name>"), TEXT("/Game/Shinobi/Characters/<Name>/RTG_MannyTo<Name>.RTG_MannyTo<Name>")}`) and rebuild. `ShinobiCoop.Hero.PickSwapsBody` checks every imported entry.

### A real head on the hero's body (Ramil, Elvin)

Ramil is the hero's body with a Tripo head scan instead of an A-pose reference: four head photos (front, back, left, right, in `SourceArt/Character/Photos/Ramil/`) go into Tripo Studio, and the textured head is exported **without** auto-rigging as `SourceArt/Character/Models/Ramil/Ramil_head.glb`. Then:

```
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python-exit-code 1 -P Scripts/character/build_head_swap.py -- Ramil
Scripts/character/build_outfits.ps1 -Body Ramil
```

`build_head_swap.py <Name>` swaps the hero's head, glasses and headband for the scan (chin on the hero's chin, the neck drawn down inside the turtleneck collar), gives it the hero's weights, so there is no Mixamo step, and tints the hands and feet to the head's skin. Ramil's Tripo hair is one smooth shell, so the script also grows about 3,000 hair cards over it: thin strips with a painted strand texture that follow the hair's flow, read from the strands in Tripo's texture. Elvin's scan already has strands painted on separate clumps (including his grey ones), so he has no cards: cards came out flat black there. It writes `<Name>_rigged.fbx`, its texture `<Name>_rigged.png` (the hero's and the head's textures on top, the cards' strand strips below) and the previews. A change to the mesh or its UVs means fitting the outfits again (`build_outfits.ps1 -Body <Name>`). Each scan has a line in `SCANS` at the top of the script (scale, chin and neck landmarks, the shirt's colour, the neck cut's slope, hair cards): measure a new one on gridded renders. A dark shirt in the scan whose neckline sits above the hero's collar at the back needs a steeper `neck_slope` (Elvin's 0.45), or its jagged edge shows above the collar. Then import just that body and its outfit copies with `$env:SETUP_HERO_ONLY="character"; $env:SETUP_HERO_CHARACTER="<Name>"; Scripts/run_python.ps1 -Script Scripts/setup_hero.py` (the full `setup_hero.py` run re-imports every body), add its roster line, and shoot its menu portrait (`Scripts/menu_shots.ps1 -Portraits`, then import only `Saved/MenuArt/T_Portrait_<Name>.png` with `setup_menu_art.py`).

Animal characters keep their tails: Mixamo skins a tail to the hips and legs, so it swings stiffly. Cloaks leave the tail outside.
