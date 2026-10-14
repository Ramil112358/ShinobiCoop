# AnimGen trial (feature/animgen)

AnimGen is UE 5.8's experimental machine-learned locomotion: a controller trained on mocap generates the pose every
frame, steered by a trajectory and a *style* (Hurried, Tired, PhoneTalking, ...). This branch tries Epic's
AnimGen Example controller on our villagers. It is off by default.

## Try it

`Shinobi.AnimGenVillagers 1` (console, or `-dpcvars=Shinobi.AnimGenVillagers=1`), then spawn villagers (the cvar is
read on spawn). Non-crowd villagers then walk with AnimGen in one of ten prop-free trained styles, picked from their
`Variant`. Crowd pedestrians keep their City Sample loops.

## How it is wired

- Plugins: `AnimGen` (experimental) and `AnimGenExample` (engine content plugin from Fab). AnimGen pulls in UAF,
  LearningCore, NNERuntimeBasicCpu, PythonMLPackages and TensorBoard.
- `/Game/Shinobi/AnimGen/ABP_AnimGenVillager`: a copy of `/AnimGenExample/Blueprints/ABP_AnimGenNPC` (controller
  `AGC_AnimGenNPC`), reparented to `UShinobiAnimGenAnimInstance`. Its event graph was rewritten (Unreal MCP,
  `write_graph_dsl`) to call `UpdateAnimGenInputs()`.
- `UShinobiAnimGenAnimInstance::UpdateAnimGenInputs` fills the AnimBP's `Properties` (style, LOD) and builds its
  `Trajectory` with `PoseSearchGenerateTransformTrajectory` from CharacterMovement. The example uses a Mover predictor
  instead; no Mover pawn is needed.
- The controller was trained on the UEFN mannequin. It runs on our hidden Quinn driver, matching bones by name, and the
  villager's character mesh follows through the usual retarget.

## Measured (`ShinobiCoop.AnimGen.*`, headless, 20 idle villagers, whole frame)

| Locomotion | ms per frame |
|---|---|
| GASP motion matching | 5.7 – 7.2 |
| AnimGen, controller LOD 0 | 10.3 – 12.8 |
| AnimGen, controller LOD 2 | 4.1 |

At LOD 2 AnimGen costs less than motion matching. `FAnimNode_AnimGenController::EvaluationPeriodInFrames`
(evaluate every Nth frame) would cut it further.

## Not done / limits

- **Props.** About 30 of the 40 trained styles hold something: phone, bags, burger, cigarette, headphones, heels.
  The example attaches its prop meshes to the UEFN mannequin's `prop_01` and other sockets. Our Quinn driver and
  generated characters have no `prop_01` bone. Supporting them means sockets on the character meshes plus the prop
  meshes (`/AnimGenExample/Characters/UEFN_Mannequin/Meshes/SM_*`).
- **Look-at** (`Properties.LookAt`) is left off.
- **Not judged by eye yet.** It needs a playtest in Lvl_London with the cvar on.
- **Experimental.** The plugin's API and assets may change between 5.8 hotfixes. Keep it on this branch until judged.

## Decision to make

If it looks better than motion matching on villagers in a playtest, use AnimGen at LOD 2 for ambient villagers. That
could include the London crowd, replacing the City Sample loops, once props are supported. Otherwise drop the branch.
