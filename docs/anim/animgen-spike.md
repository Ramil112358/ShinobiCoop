# AnimGen ambient walking

AnimGen is UE 5.8's experimental machine-learned locomotion: a controller trained on mocap generates the pose every
frame, steered by a trajectory and a *style* (Hurried, Tired, PhoneTalking, ...). Epic's AnimGen Example controller walks our
villagers and the London crowd. On by default; players and enemies keep their own AnimBP (the example controllers know
only walking: no run, jump, wall-run or montage slot).

## Switch

`Shinobi.AnimGenVillagers 0` (console, or `-dpcvars=Shinobi.AnimGenVillagers=0`) goes back to the locomotion AnimBP for
villagers and City Sample's idle/walk loops for the crowd; read on spawn. With it on, every villager walks at controller
LOD 2 (`UShinobiAnimGenAnimInstance::AmbientLOD`) in a prop-free style picked from its `Variant`: villagers from the six
that suit anyone, Londoners also from the neutral and elderly styles of their crowd character's gender.

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
- Londoners (`bCrowd`): the City Sample crowd character attaches to the hidden Quinn and its body runs
  `UShinobiRetargetAnimInstance` through `/Game/Shinobi/AnimGen/RTG_MannyToCrowd` (Scripts/anim/setup_crowd_retarget.py;
  every crowd body is on `metahuman_base_skel`). The driver ticks only while its crowd body was rendered in the last 0.5 s.

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
- **City Sample accessories.** A Londoner's random `AccessoryIndex` (bag, phone, cup) isn't matched to the style that
  holds one; matching them would unlock the prop styles for the crowd.
- **Not judged by eye yet.** Needs a playtest in Lvl_London and the valley.
- **Experimental.** The plugin's API and assets may change between 5.8 hotfixes; the cvar is the way back.
