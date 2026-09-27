# Rigging the hero on Mixamo (about 2 minutes)

You need: `SourceArt/Character/Mesh/hero_prep.fbx` (made by `Scripts/character/prep_mesh.py`) and a free Adobe account.

1. Go to https://www.mixamo.com and sign in.
2. Click **Upload Character** and drop in `hero_prep.fbx`. Wait for "Auto-Rigger" to open. If the model appears lying down or facing away, use the rotate arrows under the preview until it stands facing you, then click **Next**.
3. **Place the markers** on the front view:
   - **Chin:** on the chin (below the mouth, not on the neck).
   - **Wrists:** on the wrist joint, where the hand meets the forearm, not on the gloves' fingers.
   - **Elbows:** on the elbow joints.
   - **Knees:** on the knee joints. With the cloak, place them where the knees would be under it; the cloak should end above them.
   - **Groin:** where the legs meet.
   Leave **Use Symmetry** on.
4. **Skeleton LOD:** choose **Standard Skeleton (65)**. That one includes the fingers, which matter for hand seals later. Click **Next** and wait for the rig.
5. In the preview the character now plays a default animation. Check that the elbows, knees and head bend in the right places, and that the cloak and hands don't stretch strangely. If they do, go **Back** and move the markers.
6. Click **Download** with these settings:
   - Format: **FBX Binary (.fbx)**
   - Skin: **With Skin**
   - Frames per second: 30
   - Keyframe Reduction: none
   - Pose: **Original Pose** (if the option is shown). **Don't pick an animation first**, so the download holds only the rigged character.
7. Save the file as `SourceArt/Character/Mesh/hero_rigged.fbx`, replacing any older one.
8. Tell Claude it's there. `Scripts/setup_hero.py` imports it and replaces the stand-in mannequin in the game.

**If Mixamo says the upload failed or can't be rigged:** the mesh probably has separate floating pieces, or the arms touch the body. Tell Claude. The prep script can be re-run with fixes, for example a wider arm gap.
