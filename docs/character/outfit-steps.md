# Outfits: cloaks worn over any character

An outfit (the Hokage and Akatsuki cloaks so far) is worn on top of a roster character's body. Each player picks one under **Settings → Outfit** (`None`, `Hokage`, `Akatsuki`), and everyone sees it. The research behind this design is in [outfit-fitting-research.md](outfit-fitting-research.md).

## How it works

Like the big games' skins, one master garment becomes **one copy fitted to each body**:

- **Fitted copies.** `Scripts/character/skin_outfit.py` fits the master mesh to each roster body (Hero, Bika, Cat, Misha, Pabu): it scales it to the body, deepens it front to back so it clears the chest and hips, lifts it off the skin, splits the sleeves from the coat at the armpit, opens the back for a tail, and copies the body's skin weights. The result is `<Outfit>_<Body>_outfit.fbx`, imported as `/Game/Shinobi/Outfits/<Outfit>/SK_<Outfit>_<Body>`.
- **Hide mask.** `bake_hide_mask.py` marks what of the body the copy covers in every test pose (`<Outfit>_<Body>_hide.png`, black = hidden). In the game the body's `M_Character` material multiplies its opacity by it, so no arm or hip shows through the cloak. The head, neck, hands, feet and tail always stay shown.
- **Cloth.** The hem and sleeves are Chaos cloth. The fitter paints vertex colours: red = hem share, green = sleeve share. `UShinobiOutfitBuilderLibrary::BuildCloth` turns them into a Max Distance map (hem up to 25 cm, sleeves up to 12 cm; shoulders and chest 0, pinned). The cloth collides with a cloth-only physics asset per body (`PA_<Body>_Cloth`, with a tail capsule on Cat and Misha), not with the ragdoll.
- **In the game.** `AShinobiPlayerCharacter::ApplyOutfit` puts the copy for the worn body on `OutfitMesh` (leader-pose: it follows `HeroMesh`'s bones) and sets the hide mask. The cloth hangs along the character's gravity, even when wall-running. It is reset on teleports (more than 150 cm in one frame) and sleeps when far away or off screen. A body with no copy for the outfit wears nothing (logged once).
- The pick is saved per machine (`Outfit=` in `GameUserSettings.ini`), sent to the server by `AShinobiPlayerController::ShinobiOutfit` and replicated on the pawn (`OutfitIndex`). `ShinobiOutfit Akatsuki` in the console does the same for one session.

## Making a new outfit

1. **References.** In one ChatGPT chat, make a front view and a back view of the garment alone, as if worn by an invisible person: hollow sleeves and collar, plain light-grey background, sleeves held well out (about 40° from straight down: Tripo fuses sleeves that hang close to the coat into its sides). Save them as `SourceArt/Character/Reference/Cloak_<Name>_front.png` and `_back.png` (not committed).
2. **3D model.** Tripo Studio, multi-view, HD Model. Download the FBX and prep it:
   ```
   "C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b -P Scripts/character/prep_outfit.py -- <tripo.fbx> SourceArt/Character/Outfits/<Name>/<Name>_tripo.glb
   ```
   It opens the hem Tripo closes, decimates to 50k triangles and keeps only the base colour. Tripo wraps design elements cut off at a reference's silhouette round the sides, and embosses them into the cloth. For the Akatsuki's lower clouds, `fix_cloak_clouds.py -- <Name>_tripo.glb SourceArt/Character/Reference/Cloak_akatsuki_back.png <Name>_clouds.glb` smooths that relief and repaints the clouds (positions and sizes are constants at its top). The build uses `<Name>_clouds.glb` if there is one, otherwise `<Name>_tripo.glb`, otherwise `<Name>.glb`.
3. **Fit it to every body:**
   ```
   Scripts/character/build_outfits.ps1 -Outfit <Name>
   ```
   For each body it fits the copy, bakes the hide mask and runs `check_outfit.py`. It prints one `RESULT PASS`/`FAIL` line per body (`-Body Cat` for one body). Logs and renders of every check pose go to `Saved/OutfitCheck/`, and `fit_<Body>_front.png` / `_side.png` next to the outfit. Look at them.
4. **Import.** Close the Unreal Editor, then:
   ```
   $env:SETUP_HERO_ONLY="outfits"; Scripts/run_python.ps1 -Script Scripts/setup_hero.py
   ```
   This imports every copy with its cloth and hide mask and leaves the bodies alone. Without `SETUP_HERO_ONLY` the full run re-imports the bodies as well.
5. **Register.** Add `{TEXT("<Name>")}` to `AShinobiPlayerCharacter::Outfits`, rebuild, and run `Scripts/run_tests.ps1 -Filter ShinobiCoop.Outfit`. `EveryCopyHasCloth` checks every outfit has a copy with cloth and a hide mask on every body. `CharacterMaterialCompiles` checks the body material still compiles.
6. **Look at it in the game:** `Scripts/shot_chakra.ps1 -Outfit <Name>` films every body from the front, side and back. It shoots idle, walking, running, mid-jump and settled again (about 2 minutes), into `Saved/Screenshots/WindowsEditor/ShinobiOutfitShot_<Name>_<Body>_*.png`.
7. Commit the outfit folder's `*_outfit.fbx`, `*_outfit.png` and `*_hide.png` files, and the Content assets (see `.gitignore`).

### What the check allows

Per pose (`body_landmarks.POSES`: idle, arms down, arms out, arms up 120°, arms forward 80°, jump tuck), on the copy as skinned, before the cloth moves:

| Metric | Meaning | Limit |
|---|---|---|
| stretched | pinned edges over 3x their rest length and 2 cm longer | 50 (arms up / forward: 300) |
| exposed | hidden body vertices no longer under the cloak (see-through) | 0.5% of the hidden vertices |
| poke | shown torso, upper-arm and thigh points through the cloak in view | 40 (arms up: 150, arms forward: 100) |

Raising the arms far always webs a skinned armpit a little, so those poses get room. The game shots judge them.

## Tuning the cloth

The cloth starts from defaults in `UShinobiOutfitBuilderLibrary::BuildCloth`. Tune it on the Hero's copy, then copy that to the other bodies:

1. Close the game. Open the editor. In the Content Browser, open `/Game/Shinobi/Outfits/<Outfit>/SK_<Outfit>_Hero`.
2. **Window → Clothing.** Select the clothing asset in the **Clothing** panel.
3. In the toolbar, click **Activate Cloth Paint**. In **Cloth Painting**, choose the **Max Distance** map and set **Paint Value**. A value is how far (cm) a vertex may swing from the body; 0 pins it. Today the hem goes up to 25 and the sleeves up to 12.
4. Paint with the **Brush**, or use **Fill** or **Smooth**. Shift-drag paints 0.
5. **Preview:** press **Play** in the viewport toolbar. In the **Preview Scene Settings** tab, under **Animation**, pick a walk, run or jump animation, and watch the hem and sleeves.
6. **Stiffness and swing:** select the clothing asset and edit its **Config** (Chaos Cloth Config) in the Clothing panel. Today's defaults:
   - **Edge Stiffness** 1: how much the cloth resists stretching (1 = barely stretches).
   - **Bending Stiffness** 0.8: how much it resists folding. Lower crumples the idle hem into strips.
   - **Anim Drive Stiffness** 0.15, **Anim Drive Damping** 0.2: a pull back to the fitted shape. Higher keeps the shape and swings less, lower swings freely.
   - **Damping** 0.1: how fast swinging dies down.
   - **Drag** 0.07, **Lift** 0.07: air resistance. Higher trails more behind a run.
   - **Linear / Angular Velocity Scale** 0.6: how much of the character's own movement and turning the cloth feels. 1 is all of it (whippy), 0 none.
   - **Density** 0.35: weight per area. Heavier hangs straighter.
7. Click **Deactivate Cloth Paint**, then save.
8. Close the editor. Copy the tuning to the other bodies' copies (it is also kept in `Tuning/` so a later re-import keeps it):
   ```
   $env:SETUP_HERO_ONLY="cloth"; $env:SETUP_HERO_CLOTH_SOURCE="<Outfit>_Hero"; Scripts/run_python.ps1 -Script Scripts/setup_hero.py
   ```
   Each copy takes the value of the nearest vertex in the master garment, so the same part of the cloak gets the same tuning on every body.
9. Check it in the game: `Scripts/shot_chakra.ps1 -Outfit <Outfit>`.

## Gotchas

- **Rig scale.** Mixamo FBXs (and Blender's metre FBXs) arrive in Unreal with the Hips bone scaled 100. Animation copes, but cloth and the physics-asset generator size things in bone units: the cloak blew up into a 2 m blob and the auto ragdoll got 50 cm capsules. `Scripts/character/clean_rig.py` re-exports a rig from a centimetre scene so every bone imports at scale 1. `setup_hero.py` runs it on every body (into `Saved/CleanRigs/`), and `skin_outfit.py` exports the copies the same way.
- **The body material.** Every texture parameter in `M_Character` must have a sampler that suits its texture. A mismatch fails the whole material, and the game then draws every roster body (except the hero, which has `M_Hero`) and every outfit in the grey default material. The hide masks are therefore plain colour textures on a Color sampler.
- **Grey shots.** An uncooked `-game` run builds new meshes and textures in the background. The outfit shot preloads them and waits, but other shot modes can catch them grey on a first run after an import.

## MetaHuman bodies

RamilMH (a MetaHuman, `Scripts/setup_metahuman.py`) wears the same cloaks. The differences:

- **Its source body** is the built body exported from Unreal and cleaned: `SourceArt/Character/Models/RamilMH/RamilMH_rigged.fbx` (from `RamilMH_ue.fbx` through `clean_rig.py`, which drops the 0.01-scaled empty Unreal hangs the rig under). Re-export it after a MetaHuman rebuild changes the body.
- **Bone names.** `body_landmarks.suffix` maps MetaHuman bones onto the Mixamo roles the scripts use (`pelvis` = Hips, `upperarm_l` and its twist and corrective bones = LeftArm, ...). Of several bones with one role, `find_bone` takes the one nearest the root.
- **Corrective bones.** A MetaHuman's correctives (under `*_correctiveRoot_*`) reach far onto the neck and hips, and only follow their limb part of the way when its rig solver runs, which ours doesn't. `skin_outfit.py` moves their weight on the coat to the limb's parent (clavicle, pelvis), or the collar and waist tear when an arm or leg moves.
- **Bent elbows.** A MetaHuman binds with its elbows bent about 37° forward, and the game's arms hang straight. `skin_outfit.py` turns each sleeve onto the straightened arm (`body_landmarks.straight_hand`), and the test poses straighten the elbow first (`ELBOW_STRAIGHTEN_MIN`). Turned to the bent-forward hand, the sleeve hung in front of the upper arm, and the game's arm roll swung it off behind the arm.
- **Cloth collision.** The capsules come from the skin weights. A MetaHuman's arm bones carry none, so the clavicle's capsule wrapped the bind pose's upper arm and pushed the sleeves into spikes. `BuildClothCollision` drops any capsule that doesn't touch its own bone.
- **Hide mask.** MetaHuman skin has no opacity: the `bodyhide` stage of `setup_metahuman.py` (run by every build) puts the body's material instance on `M_RamilMH_BodySkin`, a masked copy of the skin material with `HideMask`.
- **Import just its copies:** `$env:SETUP_HERO_ONLY="outfits"; $env:SETUP_HERO_OUTFIT_BODY="RamilMH"; Scripts/run_python.ps1 -Script Scripts/setup_hero.py`. Its cloth collision is `/Game/Shinobi/MetaHumans/RamilMH/PA_RamilMH_Cloth`.

## Cloth collision

`BuildClothCollision` makes each body's `PA_<Body>_Cloth` from its skin weights, then shrinks every capsule to sit inside the skin: the 25th percentile of its own skin's distance from the capsule's axis. Fitted to the skin's bounds, the hip and belly capsules stood out of the body and blew the coat out into a ball. A little cloth sinking into skin the coat hides doesn't show.

Until 2026-10-23 it lost every rebuild of an existing asset: the new object was overwritten by the old file when the package loaded. Every body has been rebuilt since.

## Limits

- One outfit at a time. Shirts, jumpers and glasses fit the same way (a garment over the body, its own copy per body), but none has been made yet. Glasses would need no cloth (paint no red or green).
- A new roster body needs its copies: add it to `build_outfits.ps1`'s body list and `setup_hero.py`'s `ROSTER`, then run steps 3 and 4. Until then it wears nothing.
- Outfits are for players only. Enemies and villagers wear only their bodies.
- No self-collision: a sleeve can pass through the coat in an extreme pose.
