# Adding a roster character from VRoid Studio

The roster is Hero, Naruto, Sasuke, Itachi, Minato, Obito and Madara (`AShinobiPlayerCharacter::Roster`). Each player picks one under **Settings → Character**. Until a character's model is imported, picking it shows the Hero.

You need VRoid Studio (free, from vroid.com).

1. Make the character in VRoid Studio: hair, face, outfit. Keep the default body proportions. The game scales every character to 180 cm, like the other players.
2. Click **Export → Export as VRM**. Choose **VRM 1.0**; 0.x also works. Fill in the title and author, then export.
3. Save the file as `SourceArt/Character/VRoid/<Name>.vrm`, where `<Name>` is exactly one of `Naruto`, `Sasuke`, `Itachi`, `Minato`, `Obito`, `Madara`. Replace any older file.
4. Close the Unreal Editor and run:
   ```
   Scripts/run_python.ps1 -Script Scripts/setup_hero.py
   ```
   This does two things:
   - It converts every new or changed `.vrm` with Blender (`Scripts/character/vroid_to_fbx.py`). The results go to `SourceArt/Character/VRoid/<Name>/`: `<Name>.fbx` plus one PNG per material.
   - It imports each character into `/Game/Shinobi/Characters/<Name>/`.

   Look for `[setup_hero] built .../RTG_MannyTo<Name>` in the output.

The `.vrm` files stay on your machine (`.gitignore`). The converted FBX and PNGs are committed, so another machine can import them without the `.vrm`.

**To add a new name**, add a line to `Roster` in `Source/ShinobiCoop/Shinobi/ShinobiPlayerCharacter.h` and rebuild.

**Licences:** you own characters you make in VRoid Studio. Hair, clothing or texture packs bought on BOOTH or elsewhere have their own terms, so check them before sharing a build. Characters modelled on an existing franchise are fan art: fine in a private build, not in a public or commercial release.

**Not carried over from VRoid:** hair and skirt physics (the hair stays static), facial expressions, and MToon toon shading. Characters use the game's lit look plus the outline.
