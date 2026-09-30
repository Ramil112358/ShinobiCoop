# Outfits: cloaks worn over any character

An outfit is a separate skeletal mesh (the Hokage and Akatsuki cloaks so far) that any roster character wears on top of their body. It is one mesh per outfit, not one per character per outfit. Each player picks one under **Settings → Outfit** (`None`, `Hokage`, `Akatsuki`), and everyone sees it.

## How it works

- The cloak is skinned to the Mixamo skeleton (`Scripts/character/skin_outfit.py`). In the game, `AShinobiPlayerCharacter::OutfitMesh` follows `HeroMesh` through leader-pose: each cloak bone takes the body bone with the same name, so the cloak moves with whichever character is worn and needs no animation of its own.
- Each body's skeleton sits a little differently in its FBX, so `ApplyOutfit` moves the cloak by the difference between the two Hips bones' reference-pose positions.
- The cloak is outlined like the body (same custom-depth stencil) and has no collision.
- The pick is saved per machine (`Outfit=` in `GameUserSettings.ini`), sent to the server by `AShinobiPlayerController::ShinobiOutfit` and replicated on the pawn (`OutfitIndex`), like the character pick. `ShinobiOutfit Akatsuki` in the console does the same for a session.

## Making a new outfit

1. **References.** In one ChatGPT chat, make a front view and a back view of the garment alone, as if worn by an invisible person: an open-front long coat, hollow sleeves and collar, inner lining visible, sleeves about 45° down, plain light-grey background, hem above the knee. Save them as `SourceArt/Character/Reference/Cloak_<Name>_front.png` and `_back.png` (this folder is not committed).
2. **3D model.** Upload both to Hyper3D Rodin (Gen-2.5 Medium, Raw, 50k polygons, GLB) with a prompt that says "one 3D garment from two views, front and back, on its own with no body inside". Save the GLB as `SourceArt/Character/Outfits/<Name>/<Name>.glb`.
3. **Skin it.** Blender needs a rigged body for the skeleton and skin weights; use the cloakless Hero:
   ```
   "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P Scripts/character/skin_outfit.py -- SourceArt/Character/Models/Hero/Hero_rigged.fbx SourceArt/Character/Outfits/<Name>/<Name>.glb SourceArt/Character/Outfits/<Name> <Name>
   ```
   It scales the cloak to the body (collar at the head bone, hem just above the knees), copies the body's skin weights to it, eases the legs' pull on the skirt so it doesn't tear when walking, and writes `<Name>_outfit.fbx`, `<Name>_outfit.png` (its texture) and `fit_front.png` / `fit_side.png`. Open the fit images and check the cloak hangs right over the body. It also checks that the FBX keeps the body's bone orientations, because a cloak whose bones differ would be twisted in the game.
4. **Import.** Close the Unreal Editor and run `Scripts/run_python.ps1 -Script Scripts/setup_hero.py`. Each `<Name>_outfit.fbx` becomes `/Game/Shinobi/Outfits/<Name>/SK_<Name>`. To import only the outfits and leave every body alone, set `SETUP_HERO_ONLY=outfits` first.
5. **Register.** Add a line to `AShinobiPlayerCharacter::Outfits` (`{TEXT("<Name>"), TEXT("/Game/Shinobi/Outfits/<Name>/SK_<Name>.SK_<Name>")}`), rebuild, and run `Scripts/run_tests.ps1 -Filter ShinobiCoop.Hero`. `OutfitFollowsBodyAndComesOff` checks every imported outfit over every imported roster character.
6. Commit `<Name>_outfit.fbx` and `<Name>_outfit.png` (see `.gitignore`).

## Limits

- One cloak fits every body only approximately: a wider body (the animal characters) can poke through it, and tails come out through the back. Fixes: a second, wider cloak per body type, or per-character tuning.
- A cloak doesn't move by itself: no cloth physics, it just follows the bones.
- Outfits are for players only. Enemies and villagers wear only their bodies.
