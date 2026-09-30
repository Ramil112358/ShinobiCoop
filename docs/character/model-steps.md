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

Two more are planned but not made yet: Kaito (a third Rogue Ninja) and Sen (an elder). Rodin's text-only output for them either couldn't be rigged (arms fused to the body, a staff fused to the hand) or ignored the brief. Make them from a front-view concept image instead, using Rodin's image reference. No character appears on both sides; `ShinobiCoop.Cast.NoFaceOnTwoSides` checks this. The player roster (`AShinobiPlayerCharacter::Roster`) is separate. Until a character is rigged and imported, its NPC stays the tinted mannequin.

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
